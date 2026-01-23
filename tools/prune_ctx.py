#!/usr/bin/env python3
"""
Prune ctx.c to only include types used by a specific function.
Uses pycparser to parse the preprocessed context and extract dependencies.

Usage: python3 tools/prune_ctx.py <function_name> [-o output_file]
"""

import argparse
import sys
import os

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(script_dir, 'decomp-permuter/src'))

from pycparser import c_parser, c_ast, c_generator


class TypeDepsCollector(c_ast.NodeVisitor):
    """Collect all type/identifier references from an AST node."""
    def __init__(self):
        self.ids = set()

    def visit_ID(self, node):
        self.ids.add(node.name)

    def visit_IdentifierType(self, node):
        for name in node.names:
            self.ids.add(name)

    def visit_Struct(self, node):
        if node.name:
            self.ids.add(node.name)
        self.generic_visit(node)

    def visit_Union(self, node):
        if node.name:
            self.ids.add(node.name)
        self.generic_visit(node)

    def visit_Enum(self, node):
        if node.name:
            self.ids.add(node.name)
        self.generic_visit(node)


def get_node_name(node):
    """Get the name associated with an AST node."""
    if isinstance(node, c_ast.Typedef):
        return node.name
    elif isinstance(node, c_ast.FuncDef):
        return node.decl.name
    elif isinstance(node, c_ast.Decl):
        if node.name:
            return node.name
        if isinstance(node.type, (c_ast.Struct, c_ast.Union, c_ast.Enum)):
            return node.type.name
    return None


def collect_deps(node):
    """Collect all type dependencies from a node."""
    collector = TypeDepsCollector()
    collector.visit(node)
    return collector.ids


def prune_context(ctx_file, func_name, output_file=None):
    """Prune context file to only include types needed by func_name."""

    # Read and separate macros from code
    with open(ctx_file, 'r') as f:
        all_lines = f.readlines()

    macros = [l for l in all_lines if l.startswith('#define')]
    code_lines = [l for l in all_lines if not l.startswith('#')]
    code = ''.join(code_lines)

    # Parse
    parser = c_parser.CParser()
    try:
        ast = parser.parse(code)
    except Exception as e:
        print(f"Parse error: {e}", file=sys.stderr)
        sys.exit(1)

    # Find target function
    target_func = None
    for node in ast.ext:
        if isinstance(node, c_ast.FuncDef) and node.decl.name == func_name:
            target_func = node
            break

    if not target_func:
        print(f"Function '{func_name}' not found in {ctx_file}", file=sys.stderr)
        print("Available functions:", file=sys.stderr)
        for node in ast.ext:
            if isinstance(node, c_ast.FuncDef):
                print(f"  {node.decl.name}", file=sys.stderr)
        sys.exit(1)

    # Map names to declarations
    name_to_decl = {}
    for node in ast.ext:
        name = get_node_name(node)
        if name:
            name_to_decl[name] = node

    # Get direct deps from function
    needed = collect_deps(target_func)

    # Transitively collect all needed types
    to_process = list(needed)
    all_needed = set()
    while to_process:
        name = to_process.pop()
        if name in all_needed:
            continue
        all_needed.add(name)
        if name in name_to_decl:
            for dep in collect_deps(name_to_decl[name]):
                if dep not in all_needed:
                    to_process.append(dep)

    # Generate pruned output
    gen = c_generator.CGenerator()
    output = []

    # Header
    output.append(f"// Pruned context for {func_name}\n")
    output.append(f"// Generated from {ctx_file}\n\n")

    # Add relevant macros
    macro_prefixes = ['NU_SC_', 'OS_MESG_', 'OS_IM_', 'NU_DEB_', 'OS_FLAG_']
    relevant_macros = [m for m in macros if any(p in m for p in macro_prefixes)]
    if relevant_macros:
        output.append("// Relevant macros\n")
        for m in relevant_macros[:40]:
            output.append(m)
        output.append("\n")

    output.append("// Type definitions\n")

    # Output typedefs and struct decls in dependency order
    emitted = set()
    for node in ast.ext:
        name = get_node_name(node)
        if name and name in all_needed and name not in emitted:
            # Skip the target function itself for now
            if isinstance(node, c_ast.FuncDef) and node.decl.name == func_name:
                continue
            output.append(gen.visit(node) + ";\n")
            emitted.add(name)

    # Output the function implementation
    output.append(f"\n// Function implementation\n")
    output.append(gen.visit(target_func) + "\n")

    result = ''.join(output)

    # Output
    if output_file:
        with open(output_file, 'w') as f:
            f.write(result)
        print(f"Saved to {output_file} ({len(result.splitlines())} lines)")
    else:
        print(result)


def main():
    parser = argparse.ArgumentParser(
        description="Prune ctx.c to only include types used by a specific function"
    )
    parser.add_argument("func_name", help="Name of the function to extract")
    parser.add_argument("-c", "--ctx", default="ctx.c", help="Context file (default: ctx.c)")
    parser.add_argument("-o", "--output", help="Output file (default: <func_name>.ctx.c)")
    args = parser.parse_args()

    output = args.output or f"{args.func_name}.ctx.c"
    prune_context(args.ctx, args.func_name, output)


if __name__ == "__main__":
    main()
