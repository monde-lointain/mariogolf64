#!/usr/bin/env python3
"""Preprocess C headers and extract type definitions for Ghidra import."""

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from pycparser import c_parser, c_ast

# Project root (parent of tools/)
PROJECT_ROOT = Path(__file__).parent.parent

# Include paths from Makefile + src directories
INCLUDE_PATHS = [
    PROJECT_ROOT / "include",
    PROJECT_ROOT / "lib/ultralib/include",
    PROJECT_ROOT / "lib/ultralib/include/PR",
    PROJECT_ROOT / "lib/ultralib/include/gcc",
    PROJECT_ROOT / "lib/ultralib/src",
    PROJECT_ROOT / "lib/ultralib/src/audio",
    PROJECT_ROOT / "lib/ultralib/src/os",
    PROJECT_ROOT / "lib/ultralib/src/io",
    PROJECT_ROOT / "lib/ultralib/src/gu",
    PROJECT_ROOT / "lib/ultralib/src/libc",
    PROJECT_ROOT / "lib/nusys/include",
    PROJECT_ROOT / "lib/nusys/src/nusys-2.06/nusys",
    PROJECT_ROOT / "lib/nusys/src/nusys-2.06/nualsgi",
    PROJECT_ROOT / "lib/nusys/src/nusys-2.06/nualstl3",
]

# Headers to parse (entry points) - ultratypes first for base types
ENTRY_HEADERS = [
    # Base types first
    "lib/ultralib/include/PR/ultratypes.h",
    # Project headers
    "include/common.h",
    # libultra public headers
    "lib/ultralib/include/PR/os.h",
    "lib/ultralib/include/PR/libaudio.h",
    "lib/ultralib/include/PR/gu.h",
    # libultra internal headers (src/)
    "lib/ultralib/src/audio/synthInternals.h",
    "lib/ultralib/src/audio/seq.h",
    "lib/ultralib/src/audio/seqp.h",
    "lib/ultralib/src/audio/sndp.h",
    "lib/ultralib/src/audio/cseq.h",
    "lib/ultralib/src/audio/cseqp.h",
    "lib/ultralib/src/os/osint.h",
    "lib/ultralib/src/io/viint.h",
    "lib/ultralib/src/io/controller.h",
    "lib/ultralib/src/gu/guint.h",
    "lib/ultralib/src/libc/xstdio.h",
    # nusys headers
    "lib/nusys/include/nusys.h",
    "lib/nusys/include/nualsgi.h",
    # nualstl requires libmus.h which is not available
    # nusys internal headers (src/)
    "lib/nusys/src/nualsgi/nualsgi_n.h",
    # Note: C files with inline type defs are not included because pycparser
    # can't handle function bodies. Types in C files should be moved to headers.
]


def parse_args():
    parser = argparse.ArgumentParser(description="Parse C headers for Ghidra import")
    parser.add_argument("-o", "--output", default="build/types.json", help="Output JSON file")
    return parser.parse_args()


def create_combined_header(headers: list[str]) -> str:
    """Create temp file that includes all target headers."""
    lines = [
        "/* Auto-generated combined header for parsing */",
        "#define _LANGUAGE_C",
        "#define _MIPS_SZLONG 32",
        "",
    ]
    for h in headers:
        lines.append(f'#include "{h}"')
    return "\n".join(lines)


def get_source_location(node) -> str | None:
    """Extract original source file from #line directives."""
    if not node.coord:
        return None
    # coord.file contains the path from #line directive
    return node.coord.file


def categorize_path(filepath: str | None) -> str:
    """Determine Ghidra category from source file path."""
    if not filepath:
        return "/mariogolf64"

    fp = filepath.lower()
    if "/pr/" in fp or "ultralib" in fp or "ultratypes" in fp:
        return "/libultra"
    if "nusys" in fp:
        return "/nusys"
    return "/mariogolf64"


class TypeExtractor(c_ast.NodeVisitor):
    """Extract type definitions from AST."""

    def __init__(self):
        self.types = []
        self.seen = set()

    def visit_Typedef(self, node):
        name = node.name
        if name in self.seen:
            return
        self.seen.add(name)

        category = categorize_path(get_source_location(node))

        type_node = node.type
        if isinstance(type_node, c_ast.TypeDecl):
            inner = type_node.type
            # Handle anonymous struct/union defined in typedef
            if isinstance(inner, c_ast.Struct) and inner.decls:
                self._handle_anon_struct(inner, name, category)
            elif isinstance(inner, c_ast.Union) and inner.decls:
                self._handle_anon_union(inner, name, category)
            else:
                base = self._get_base_type(inner)
                self.types.append({
                    "kind": "typedef",
                    "name": name,
                    "base": base,
                    "category": category,
                })
        elif isinstance(type_node, c_ast.ArrayDecl):
            self._handle_array_typedef(node, name, category)
        elif isinstance(type_node, c_ast.PtrDecl):
            self._handle_ptr_typedef(node, name, category)

    def visit_Decl(self, node):
        """Handle struct/enum/union declarations."""
        if node.type is None:
            return

        # Check for struct/union/enum definitions
        type_node = node.type
        if isinstance(type_node, c_ast.Struct):
            self._handle_struct(type_node)
        elif isinstance(type_node, c_ast.Union):
            self._handle_union(type_node)
        elif isinstance(type_node, c_ast.Enum):
            self._handle_enum(type_node)

    def _get_base_type(self, node) -> str:
        """Convert type node to string representation."""
        if isinstance(node, c_ast.IdentifierType):
            return " ".join(node.names)
        elif isinstance(node, c_ast.Struct):
            return f"struct {node.name}" if node.name else "struct"
        elif isinstance(node, c_ast.Union):
            return f"union {node.name}" if node.name else "union"
        elif isinstance(node, c_ast.Enum):
            return f"enum {node.name}" if node.name else "enum"
        elif isinstance(node, c_ast.PtrDecl):
            return self._get_base_type(node.type) + "*"
        return "unknown"

    def _handle_struct(self, node):
        if not node.name or node.name in self.seen:
            return
        if not node.decls:  # Forward declaration
            return
        self.seen.add(node.name)

        category = categorize_path(get_source_location(node))
        fields = []
        for decl in node.decls:
            if decl.name:
                fields.append({
                    "name": decl.name,
                    "type": self._get_decl_type(decl),
                })

        self.types.append({
            "kind": "struct",
            "name": node.name,
            "fields": fields,
            "category": category,
        })

    def _handle_union(self, node):
        if not node.name or node.name in self.seen:
            return
        if not node.decls:
            return
        self.seen.add(node.name)

        category = categorize_path(get_source_location(node))
        fields = []
        for decl in node.decls:
            if decl.name:
                fields.append({
                    "name": decl.name,
                    "type": self._get_decl_type(decl),
                })

        self.types.append({
            "kind": "union",
            "name": node.name,
            "fields": fields,
            "category": category,
        })

    def _handle_enum(self, node):
        if not node.name or node.name in self.seen:
            return
        if not node.values:
            return
        self.seen.add(node.name)

        category = categorize_path(get_source_location(node))
        members = []
        value = 0
        for enumerator in node.values.enumerators:
            if enumerator.value:
                # Try to evaluate constant
                value = self._eval_const(enumerator.value)
            members.append([enumerator.name, value])
            value += 1

        self.types.append({
            "kind": "enum",
            "name": node.name,
            "members": members,
            "category": category,
        })

    def _handle_anon_struct(self, node, typedef_name, category):
        """Handle anonymous struct defined in typedef."""
        fields = []
        for decl in node.decls:
            if decl.name:
                fields.append({
                    "name": decl.name,
                    "type": self._get_decl_type(decl),
                })

        self.types.append({
            "kind": "struct",
            "name": typedef_name,
            "fields": fields,
            "category": category,
        })

    def _handle_anon_union(self, node, typedef_name, category):
        """Handle anonymous union defined in typedef."""
        fields = []
        for decl in node.decls:
            if decl.name:
                fields.append({
                    "name": decl.name,
                    "type": self._get_decl_type(decl),
                })

        self.types.append({
            "kind": "union",
            "name": typedef_name,
            "fields": fields,
            "category": category,
        })

    def _get_decl_type(self, decl) -> str:
        """Get type string from declaration."""
        return self._type_to_string(decl.type)

    def _type_to_string(self, node) -> str:
        if isinstance(node, c_ast.TypeDecl):
            return self._type_to_string(node.type)
        elif isinstance(node, c_ast.IdentifierType):
            return " ".join(node.names)
        elif isinstance(node, c_ast.PtrDecl):
            return self._type_to_string(node.type) + "*"
        elif isinstance(node, c_ast.ArrayDecl):
            base = self._type_to_string(node.type)
            dim = self._eval_const(node.dim) if node.dim else 0
            return f"{base}[{dim}]"
        elif isinstance(node, c_ast.Struct):
            return f"struct {node.name}" if node.name else "struct"
        elif isinstance(node, c_ast.Union):
            return f"union {node.name}" if node.name else "union"
        elif isinstance(node, c_ast.Enum):
            return f"enum {node.name}" if node.name else "enum"
        elif isinstance(node, c_ast.FuncDecl):
            return self._func_to_string(node)
        return "unknown"

    def _func_to_string(self, node) -> str:
        """Convert function declaration to string representation."""
        # Get return type
        ret_type = self._type_to_string(node.type)

        # Get parameter types
        params = []
        if node.args:
            for param in node.args.params:
                if isinstance(param, c_ast.Typename):
                    # unnamed parameter
                    params.append(self._type_to_string(param.type))
                elif isinstance(param, c_ast.Decl):
                    params.append(self._type_to_string(param.type))
                elif isinstance(param, c_ast.EllipsisParam):
                    params.append("...")

        if not params:
            params = ["void"]

        return "func(%s)->%s" % (", ".join(params), ret_type)

    def _eval_const(self, node) -> int:
        """Evaluate constant expression."""
        if isinstance(node, c_ast.Constant):
            val = node.value
            if val.startswith("0x"):
                return int(val, 16)
            return int(val)
        elif isinstance(node, c_ast.UnaryOp):
            if node.op == "-":
                return -self._eval_const(node.expr)
        elif isinstance(node, c_ast.BinaryOp):
            left = self._eval_const(node.left)
            right = self._eval_const(node.right)
            if node.op == "+":
                return left + right
            elif node.op == "-":
                return left - right
            elif node.op == "*":
                return left * right
            elif node.op == "/":
                return left // right
            elif node.op == "<<":
                return left << right
            elif node.op == ">>":
                return left >> right
        return 0

    def _handle_array_typedef(self, node, name, category):
        """Handle typedef of array type."""
        base = self._type_to_string(node.type)
        self.types.append({
            "kind": "typedef",
            "name": name,
            "base": base,
            "category": category,
        })

    def _handle_ptr_typedef(self, node, name, category):
        """Handle typedef of pointer type."""
        base = self._type_to_string(node.type)
        self.types.append({
            "kind": "typedef",
            "name": name,
            "base": base,
            "category": category,
        })


def preprocess_headers(headers: list[str]) -> str:
    """Run gcc -E on combined headers, return preprocessed source."""
    combined = create_combined_header(headers)

    # Build gcc command
    cmd = ["gcc", "-E", "-"]
    for inc in INCLUDE_PATHS:
        cmd.extend(["-I", str(inc)])
    cmd.extend(["-D_LANGUAGE_C", "-D_MIPS_SZLONG=32", "-DM2CTX"])

    result = subprocess.run(
        cmd,
        input=combined,
        capture_output=True,
        text=True,
        cwd=PROJECT_ROOT,
    )

    if result.returncode != 0:
        print(f"gcc preprocessing failed:\n{result.stderr}", file=sys.stderr)
        sys.exit(1)

    return result.stdout


def parse_types(preprocessed: str) -> list[dict]:
    """Parse preprocessed C and extract type definitions."""
    parser = c_parser.CParser()
    ast = parser.parse(preprocessed)

    extractor = TypeExtractor()
    extractor.visit(ast)

    return extractor.types


def main():
    args = parse_args()

    print("Preprocessing headers...")
    preprocessed = preprocess_headers(ENTRY_HEADERS)
    print(f"Preprocessed output: {len(preprocessed)} bytes")

    print("Parsing types...")
    types = parse_types(preprocessed)
    print(f"Found {len(types)} type definitions")

    # Compute dependency order (simple: typedefs first, then enums, then structs/unions)
    order = []
    for t in types:
        if t["kind"] == "typedef":
            order.append(t["name"])
    for t in types:
        if t["kind"] == "enum":
            order.append(t["name"])
    for t in types:
        if t["kind"] in ("struct", "union"):
            order.append(t["name"])

    output = {"types": types, "order": order}

    # Write output
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(output, f, indent=2)

    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
