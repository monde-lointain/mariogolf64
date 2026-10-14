"""Bank identity and source-context invariants must survive database reconstruction."""

from __future__ import annotations

import pytest
from conftest import load_tool
from pycparser import c_ast, c_parser

ghidra = load_tool("ghidra")


def bank_config():
    return {
        "segments": [
            {
                "name": "main",
                "type": "code",
                "start": 0x1000,
                "vram": 0x80001000,
                "bss_size": 0x100,
                "subsegments": [
                    [0x1000, "asm"],
                    [0x1100, "textbin", "rsp/ucode"],
                    [0x1120, "data"],
                    {"type": "bss", "vram": 0x80001140},
                ],
            },
            {
                "name": "overlay_3",
                "type": "code",
                "start": 0x1140,
                "vram": 0x801F4A30,
                "bss_size": 0x20,
                "subsegments": [
                    [0x1140, "asm"],
                    {"start": 0x1180, "type": "bss", "vram": 0x801F4A70},
                ],
            },
            {
                "name": "overlay_8",
                "type": "code",
                "start": 0x1180,
                "vram": 0x801F4A30,
                "bss_size": 0x40,
                "subsegments": [
                    [0x1180, "asm"],
                    {"start": 0x11A0, "type": "bss", "vram": 0x801F4A50},
                ],
            },
            [0x11A0],
        ]
    }


def test_reused_vram_maps_different_rom_bytes_without_initializing_bss():
    layout = ghidra.segment_layout(bank_config())
    first, second = layout[1:]
    assert (
        first["regions"][0]["address"] == second["regions"][0]["address"] == 0x801F4A30
    )
    assert first["regions"][0]["offset"] == 0x1140
    assert second["regions"][0]["offset"] == 0x1180
    assert layout[0]["regions"][-1]["offset"] == -1
    assert layout[0]["regions"][-1]["address"] == 0x80001140
    assert layout[0]["regions"][-1]["length"] == 0x100


def test_rsp_microcode_is_not_cpu_executable():
    main = ghidra.segment_layout(bank_config())[0]
    rsp = next(region for region in main["regions"] if region["offset"] == 0x1100)
    assert not rsp["execute"]
    assert not rsp["write"]
    assert rsp["length"] == 0x20


def test_reused_function_address_requires_bank_or_rom_identity():
    records = {
        "overlay_3": [
            {
                "name": "func_ovl3_801F4A30",
                "address": 0x801F4A30,
                "rom": 0x1140,
                "segment": "overlay_3",
            }
        ],
        "overlay_8": [
            {
                "name": "func_ovl8_801F4A30",
                "address": 0x801F4A30,
                "rom": 0x1180,
                "segment": "overlay_8",
            }
        ],
    }
    manifest = {"functions": records}
    with pytest.raises(ValueError, match="qualify reused VRAM"):
        ghidra.resolve_function(manifest, "0x801F4A30")
    assert ghidra.resolve_function(manifest, "0x801F4A30", "overlay_8")["rom"] == 0x1180
    assert (
        ghidra.resolve_function(manifest, "0x801F4A30", rom=0x1140)["program"]
        == "overlay_3"
    )


def test_source_context_preserves_declarators_and_enum_values():
    source = """
        typedef struct { int value; void (*callback)(int); } Record;
        enum Mode { NEGATIVE = -1, POSITIVE = 2 };
        Record table[] = {{1, 0}, {2, 0}};
        const char *message = "brace } and semicolon ;";
        int first = sizeof(Record), second = 17;
        int read_record(Record *record) { if (record) { return record->value; } return 0; }
        void update_record(Record *record) { record->value++; }
    """
    context = ghidra.declaration_context(source)
    parsed = c_parser.CParser().parse(context)
    variables = {node.name: node for node in parsed.ext if isinstance(node, c_ast.Decl)}
    assert variables["table"].type.dim is None
    assert all(
        variables[name].init is None for name in ("table", "message", "first", "second")
    )
    record = parsed.ext[0].type.type
    assert isinstance(record.decls[1].type.type, c_ast.FuncDecl)
    mode = parsed.ext[1].type.values.enumerators
    assert isinstance(mode[0].value, c_ast.UnaryOp) and mode[0].value.op == "-"
    assert mode[0].value.expr.value == "1" and mode[1].value.value == "2"
    assert isinstance(variables["read_record"].type, c_ast.FuncDecl)
    assert isinstance(variables["update_record"].type, c_ast.FuncDecl)
    assert not any(isinstance(node, c_ast.FuncDef) for node in parsed.ext)


def test_assembly_cannot_silently_attach_to_wrong_bank(tmp_path):
    assembly = tmp_path / "wrong.s"
    assembly.write_text(
        "nonmatching func_ovl3_801F4A30, 0x8\n\nglabel func_ovl3_801F4A30\n/* 1140 801F4A34 03E00008 */ jr $ra\n"
    )
    with pytest.raises(ValueError, match="Assembly/YAML address disagreement"):
        ghidra.assembly_functions(ghidra.segment_layout(bank_config()), [tmp_path])


def test_incomplete_callback_arrays_retain_element_type_and_scope():
    context, bindings = ghidra.global_type_bindings(
        "typedef int (*Callback)(void *); static Callback callbacks[]; int count;"
    )
    callback = next(binding for binding in bindings if binding["name"] == "callbacks")
    assert callback["type_name"] == "Callback"
    assert callback["static"] and callback["incomplete_array"]
    # Incomplete arrays cannot become zero-length Ghidra typedefs or fake scalar data.
    aliases = c_parser.CParser().parse(context)
    assert all(not isinstance(node.type, c_ast.ArrayDecl) for node in aliases.ext)
    count = next(binding for binding in bindings if binding["name"] == "count")
    assert not count["static"] and not count["incomplete_array"]


def test_partial_header_definition_does_not_claim_source_function_ownership():
    definitions = set()
    context = ghidra.declaration_context(
        "int header_helper(void) { return 1; }\n"
        "int external(int value);\n"
        "actual(value)\nint value;\n{ return value + 2; }\n",
        definitions,
        {3, 4, 5},
    )
    assert definitions == {"actual"}
    parsed = c_parser.CParser().parse(context)
    actual = next(node for node in parsed.ext if isinstance(node, c_ast.FuncDef))
    assert actual.decl.name == "actual"
    assert actual.param_decls[0].name == "value"
    assert isinstance(actual.decl.type.type.type, c_ast.IdentifierType)
    assert actual.decl.type.type.type.names == ["int"]


def test_legacy_overlay_names_need_proven_bank_ownership():
    def symbol(name, *, rom=None, segment=None, source="ghidra_symbols.txt"):
        return {
            "name": name,
            "address": 0x801F4A30,
            "rom": rom,
            "segment": segment,
            "source": source,
        }

    names = [
        symbol("unknown_owner"),
        symbol("overlay_3_dispatch"),
        symbol("qualified", rom=0x1180),
        symbol("defined_in_bank"),
        symbol("overlay_3_dispatch", segment="overlay_8", source="extracted assembly"),
    ]
    ambiguous = ghidra.bind_symbol_ownership(
        names,
        ghidra.segment_layout(bank_config()),
        [{"segment": "overlay_3", "definitions": ["defined_in_bank"]}],
    )
    assert names[0]["segment"] == "ambiguous"
    assert ambiguous[0]["candidates"] == ["overlay_3", "overlay_8"]
    assert names[1]["segment"] == names[3]["segment"] == "overlay_3"
    assert names[2]["segment"] == "overlay_8"
    assert names[4]["name"] == "D_ovl8_801F4A30"
