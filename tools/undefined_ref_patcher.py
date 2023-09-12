#!/usr/bin/env python3

"""
This script takes an error message containing one or more undefined symbol
references from the MIPS Linux GNU linker (ld) and patches the addresses into a
linker script so the program can successfully link when the linker is ran again.
"""

LINKER_SCRIPT_PATH = "linker_scripts/undefined_syms_auto.ld"
ERROR_MESSAGE_PATH = "tools/error.txt"

import re

# Read error messages from a text file
with open(ERROR_MESSAGE_PATH, "r") as error_file:
    error_messages = error_file.read()

# Read linker script from a text file
with open(LINKER_SCRIPT_PATH, "r") as linker_file:
    linker_script = linker_file.read()

# Extract unique undefined reference symbols from error messages using regex
undefined_symbols = set(re.findall(r'undefined reference to `D_([0-9A-F]+)', error_messages))

# Patch the linker script with dummy references for unique undefined symbols
for symbol in undefined_symbols:
    linker_script += f"D_{symbol} = 0x{symbol};\n"

# Save the patched linker script
with open(LINKER_SCRIPT_PATH, "w") as outfile:
    outfile.write(linker_script)