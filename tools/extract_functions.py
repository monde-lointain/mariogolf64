#!/usr/bin/env python3
"""Extract and format functions from MIPS assembly files.

Library module: `extract_functions()` is imported by tools/seed_c.py to slice a
single function's formatted asm. No CLI — invoke it from Python.
"""

import re


def format_line(line: str) -> str | None:
    """Format a single line, returning None if it should be skipped."""
    stripped = line.strip()

    # Label line (e.g., "  .L80099F64:")
    label_match = re.match(r'^\s*(\.L\w+):\s*$', line)
    if label_match:
        return f"{label_match.group(1)}:"

    # Instruction line with comment (e.g., "    /* 75354 80099F54 14800003 */  bnez       $a0, .L80099F64")
    instr_match = re.match(r'^\s*/\*[^*]*\*/\s+(\S+)\s*(.*)?$', line)
    if instr_match:
        instr = instr_match.group(1)
        operands = instr_match.group(2) or ""
        # Remove $ from registers
        operands = re.sub(r'\$', '', operands)
        # Remove trailing comments
        operands = re.sub(r'\s*/\*.*\*/', '', operands)
        # Strip whitespace from operands
        operands = operands.strip()
        # Align operands to column 16 (8 indent + 8 for instruction)
        if operands:
            return f"        {instr:<11}{operands}"
        return f"        {instr}"

    return None


def extract_functions(content: str) -> list[tuple[str, str]]:
    """Extract all formatted functions from file content.

    Returns list of (func_name, formatted_content) tuples.
    """
    functions = []
    lines = content.splitlines()

    i = 0
    while i < len(lines):
        line = lines[i]

        # Look for function start
        glabel_match = re.match(r'^glabel\s+(\w+)\s*$', line)
        if glabel_match:
            func_name = glabel_match.group(1)
            func_lines = [func_name]
            i += 1

            # Collect lines until endlabel
            while i < len(lines):
                line = lines[i]
                endlabel_match = re.match(r'^endlabel\s+(\w+)\s*$', line)
                if endlabel_match:
                    break

                formatted = format_line(line)
                if formatted is not None:
                    func_lines.append(formatted)
                i += 1

            functions.append((func_name, '\n'.join(func_lines)))

        i += 1

    return functions
