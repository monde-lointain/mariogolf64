#!/usr/bin/env python3
"""Project-scoped MCP proxy: exact program identities and no arbitrary script tools.

Executed by tools/ghidra.py using the installed MCP environment, not the ROM venv.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import fcntl
import json
import os
import urllib.parse
import urllib.request
from pathlib import Path

from mcp import types
from mcp.server.lowlevel import Server
from mcp.server.stdio import stdio_server

# Administrative import/rebase/delete/server/debugger/script tools are deliberately absent.
ALLOWED = {
    "get_function_by_address",
    "get_function_count",
    "list_functions",
    "list_functions_enhanced",
    "decompile_function",
    "disassemble_function",
    "get_function_callees",
    "get_function_callers",
    "get_function_xrefs",
    "get_xrefs_to",
    "get_xrefs_from",
    "get_function_call_graph",
    "list_data_items",
    "list_globals",
    "list_strings",
    "search_strings",
    "search_functions",
    "read_memory",
    "get_address_spaces",
    "get_current_program_info",
    "get_language_metadata",
    "analysis_status",
    "list_data_types",
    "search_data_types",
    "get_type_size",
    "get_struct_layout",
    "get_enum_values",
    "list_calling_conventions",
    "get_function_labels",
    "get_comment",
    "get_function_comments",
    "list_bookmarks",
    "rename_function",
    "rename_variables",
    "set_function_prototype",
    "set_variable_type",
    "set_variable_storage",
    "create_struct",
    "create_union",
    "create_enum",
    "create_typedef",
    "create_function_signature",
    "apply_data_type",
    "modify_struct_field",
    "modify_struct_field_type",
    "set_comment",
    "set_bookmark",
    "save_program",
}


def http(
    port: int,
    path: str,
    params: dict | None = None,
    method: str = "GET",
    *,
    query: dict | None = None,
):
    url = f"http://127.0.0.1:{port}/{path.lstrip('/')}"
    data = None
    if method == "POST":
        data = json.dumps(params or {}).encode()
    else:
        query = params
    if query:
        url += "?" + urllib.parse.urlencode(query)
    headers = {"Content-Type": "application/json"}
    token = os.environ.get("GHIDRA_MCP_AUTH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urllib.request.urlopen(
        urllib.request.Request(url, data=data, headers=headers, method=method),
        timeout=180,
    ) as response:
        text = response.read().decode()
    try:
        result = json.loads(text)
    except json.JSONDecodeError:
        return text
    if isinstance(result, dict) and (
        "error" in result or result.get("success") is False
    ):
        raise ValueError(str(result))
    return result


def reviewed_script(port: int, program: str, mode: str, address: int = 0) -> dict:
    script = Path(__file__).parent / "ghidra" / "Mg64Context.java"
    result = http(
        port,
        "run_ghidra_script",
        {
            "script_name": str(script),
            "args": f"{mode} 0x{address:X}",
            "timeout_seconds": 120,
        },
        "POST",
        query={"program": program},
    )
    for line in result["console_output"].splitlines():
        if "MG64_JSON:" in line:
            return json.loads(line.split("MG64_JSON:", 1)[1])
    raise ValueError("Reviewed context script failed: " + result["console_output"])


def tool_schema(endpoint: dict) -> dict:
    properties = {}
    required = []
    for parameter in endpoint["params"]:
        name = parameter["name"]
        kind = {"integer": "integer", "boolean": "boolean", "number": "number"}.get(
            parameter["type"], "string"
        )
        properties[name] = {
            "type": kind,
            "description": parameter.get("description", ""),
        }
        if parameter.get("required") or name == "program":
            required.append(name)
    return {
        "type": "object",
        "properties": properties,
        "required": required,
        "additionalProperties": False,
    }


@contextlib.contextmanager
def request_lock(state: Path):
    with (state / "requests.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def verify_identity(port: int, manifest: dict, manifest_path: Path):
    ready = json.loads((manifest_path.parent / "ready.json").read_text())
    if ready.get("manifest_id") != manifest["manifest_id"]:
        raise ValueError(
            "Database source snapshot changed; reconnect the project MCP bridge"
        )
    identity = reviewed_script(port, "main", "identity")
    if (
        Path(identity["manifest"]).resolve() != manifest_path
        or identity["rom_sha1"] != manifest["rom_sha1"]
        or identity["manifest_id"] != manifest["manifest_id"]
    ):
        raise ValueError(
            "Ghidra service belongs to another checkout, ROM, or source snapshot"
        )


async def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()
    manifest_path = args.state_dir.resolve() / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    programs = {p["name"]: set(p["segments"]) for p in manifest["programs"]}
    with request_lock(args.state_dir):
        verify_identity(args.port, manifest, manifest_path)
        schema = http(args.port, "mcp/schema")
    endpoints = {
        t["path"].lstrip("/"): t
        for t in schema["tools"]
        if t["path"].lstrip("/") in ALLOWED
        and any(p["name"] == "program" for p in t["params"])
    }
    server = Server("mariogolf64-ghidra")
    lock = asyncio.Lock()

    @server.list_tools()
    async def list_tools():
        tools = [
            types.Tool(
                name=name,
                description=endpoint["description"],
                inputSchema=tool_schema(endpoint),
            )
            for name, endpoint in sorted(endpoints.items())
        ]
        tools.append(
            types.Tool(
                name="mg64_context",
                description="Export live decompile, transitive C type context, callees, globals and ROM provenance. Exact program required; no files are modified.",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "program": {"type": "string", "enum": sorted(programs)},
                        "function": {
                            "type": "string",
                            "description": "Exact imported function name or hexadecimal entry address",
                        },
                    },
                    "required": ["program", "function"],
                    "additionalProperties": False,
                },
            )
        )
        tools.append(
            types.Tool(
                name="mg64_programs",
                description="List exact program names, segments and ROM identity for this database.",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "additionalProperties": False,
                },
            )
        )
        return tools

    def call(name: str, arguments: dict):
        if name == "mg64_programs":
            if arguments:
                raise ValueError("mg64_programs takes no arguments")
            return {
                "project": manifest["project_name"],
                "rom_sha1": manifest["rom_sha1"],
                "programs": manifest["programs"],
            }
        if name not in endpoints and name != "mg64_context":
            raise ValueError("Tool is not permitted by the project bridge")
        program = arguments.get("program")
        if program not in programs:
            raise ValueError(
                "An exact program name is required: " + ", ".join(sorted(programs))
            )
        opened = http(args.port, "list_open_programs")
        verify_identity(args.port, manifest, manifest_path)
        if program not in {p["name"] for p in opened["programs"]}:
            http(
                args.port, "load_program_from_project", {"path": "/" + program}, "POST"
            )
        if name == "mg64_context":
            if set(arguments) != {"program", "function"}:
                raise ValueError("mg64_context requires only program and function")
            reference = arguments["function"]
            matches = [
                f
                for segment in programs[program]
                for f in manifest["functions"][segment]
                if f["name"] == reference or reference.lower() == f"0x{f['address']:x}"
            ]
            if len(matches) != 1:
                raise ValueError("Not one exact imported function in this program")
            result = reviewed_script(
                args.port, program, "context", matches[0]["address"]
            )
            result["origin"] = {
                **matches[0],
                "program": program,
                "rom_sha1": manifest["rom_sha1"],
            }
            return result
        endpoint = endpoints[name]
        parameters = {p["name"]: p for p in endpoint["params"]}
        unknown = set(arguments) - parameters.keys()
        if unknown:
            raise ValueError("Unknown parameters: " + ", ".join(sorted(unknown)))
        for parameter in parameters.values():
            if parameter.get("required") and parameter["name"] not in arguments:
                raise ValueError("Missing parameter: " + parameter["name"])
        if endpoint["method"] == "POST":
            query = {
                key: value
                for key, value in arguments.items()
                if parameters[key]["source"] == "query"
            }
            body = {
                key: value
                for key, value in arguments.items()
                if parameters[key]["source"] == "body"
            }
            return http(args.port, endpoint["path"], body, "POST", query=query)
        return http(args.port, endpoint["path"], arguments, endpoint["method"])

    def serialized_call(name: str, arguments: dict):
        with request_lock(args.state_dir):
            return call(name, arguments)

    @server.call_tool()
    async def call_tool(name: str, arguments: dict):
        async with lock:
            result = await asyncio.to_thread(serialized_call, name, arguments)
        return [types.TextContent(type="text", text=json.dumps(result))]

    async with stdio_server() as (reader, writer):
        await server.run(reader, writer, server.create_initialization_options())


if __name__ == "__main__":
    asyncio.run(main())
