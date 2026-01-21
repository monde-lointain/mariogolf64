# Mario Golf 64 Overlay Creator for Ghidra
#
# Creates overlay address spaces for N64 overlays that share the same VMA.
# Ghidra's ELF loader clobbers overlapping sections, so this script reads
# bytes directly from the ELF file and creates proper Ghidra overlay blocks.
#
# Usage:
# 1. Import build/mariogolf64.elf into Ghidra
# 2. Run this script
# 3. Select the same ELF file when prompted
# 4. New overlay address spaces will appear in Window -> Memory Map
#
# The game has 17 overlays at 3 shared addresses:
# - 0x801F4A30: overlays 0, 3, 7, 8, 9, 10, 15, 16
# - 0x8024D800: overlays 4, 5, 6
# - 0x80241800: overlays 11, 12, 13, 14
#
# @author Claude
# @category N64
# @runtime Jython

from ghidra.program.model.mem import MemoryBlockType
from java.io import ByteArrayInputStream, File, FileInputStream
import jarray
import struct
import re

def read_file(path):
    """Read entire file as byte array."""
    f = File(path)
    size = int(f.length())
    fis = FileInputStream(f)
    try:
        jbytes = jarray.zeros(size, 'b')
        fis.read(jbytes)
    finally:
        fis.close()
    # Convert signed Java bytes to unsigned Python bytes
    return bytes(bytearray([b & 0xFF for b in jbytes]))

def parse_elf_header(data):
    """Parse ELF header, return (e_shoff, e_shnum, e_shstrndx, e_shentsize)."""
    # Verify ELF magic
    if data[0:4] != b'\x7fELF':
        raise ValueError("Not an ELF file")

    # ELF32 big-endian MIPS
    # e_shoff at offset 0x20 (4 bytes)
    # e_shentsize at offset 0x2E (2 bytes)
    # e_shnum at offset 0x30 (2 bytes)
    # e_shstrndx at offset 0x32 (2 bytes)
    e_shoff = struct.unpack('>I', data[0x20:0x24])[0]
    e_shentsize = struct.unpack('>H', data[0x2E:0x30])[0]
    e_shnum = struct.unpack('>H', data[0x30:0x32])[0]
    e_shstrndx = struct.unpack('>H', data[0x32:0x34])[0]

    return (e_shoff, e_shnum, e_shstrndx, e_shentsize)

def parse_section_header(data, offset):
    """Parse single section header at offset.
    Returns dict with: name_idx, type, flags, addr, offset, size."""
    fields = struct.unpack('>IIIIIIIIII', data[offset:offset+40])
    return {
        'name_idx': fields[0],   # sh_name - index into string table
        'type': fields[1],       # sh_type
        'flags': fields[2],      # sh_flags
        'addr': fields[3],       # sh_addr (VMA)
        'offset': fields[4],     # sh_offset (file offset)
        'size': fields[5],       # sh_size
    }

def get_section_name(data, strtab_offset, name_idx):
    """Get null-terminated string from string table."""
    start = strtab_offset + name_idx
    end = data.index(b'\x00', start)
    return data[start:end].decode('ascii')

def parse_all_sections(data):
    """Parse all section headers, return list of dicts with name resolved."""
    e_shoff, e_shnum, e_shstrndx, e_shentsize = parse_elf_header(data)

    # First, get string table section to resolve names
    strtab_hdr = parse_section_header(data, e_shoff + e_shstrndx * e_shentsize)
    strtab_offset = strtab_hdr['offset']

    sections = []
    for i in range(e_shnum):
        offset = e_shoff + i * e_shentsize
        hdr = parse_section_header(data, offset)
        hdr['name'] = get_section_name(data, strtab_offset, hdr['name_idx'])
        sections.append(hdr)

    return sections

def find_overlay_sections(sections):
    """Find overlay sections and group by VMA.
    Returns dict: vma -> [(name, file_offset, size, bss_size), ...]
    """
    # Pattern: .overlay_N (code/data) and .overlay_N_bss
    overlay_pattern = re.compile(r'^\.overlay_(\d+)$')
    bss_pattern = re.compile(r'^\.overlay_(\d+)_bss$')

    # First pass: collect all overlays
    overlays = {}  # overlay_num -> {'code': section, 'bss': section}

    for sec in sections:
        match = overlay_pattern.match(sec['name'])
        if match:
            num = int(match.group(1))
            if num not in overlays:
                overlays[num] = {}
            overlays[num]['code'] = sec
            continue

        match = bss_pattern.match(sec['name'])
        if match:
            num = int(match.group(1))
            if num not in overlays:
                overlays[num] = {}
            overlays[num]['bss'] = sec

    # Group by VMA (code section address)
    by_vma = {}  # vma -> [(overlay_num, code_sec, bss_sec), ...]

    for num in sorted(overlays.keys()):
        ovl = overlays[num]
        if 'code' not in ovl:
            continue

        code_sec = ovl['code']
        bss_sec = ovl.get('bss')
        vma = code_sec['addr']

        if vma not in by_vma:
            by_vma[vma] = []

        by_vma[vma].append((num, code_sec, bss_sec))

    return by_vma

def create_overlay_block(name, vma, data, is_bss=False):
    """Create a Ghidra overlay memory block.

    Args:
        name: Block name (e.g., 'overlay_3')
        vma: Virtual memory address
        data: Byte data (ignored if is_bss=True)
        is_bss: If True, create uninitialized block

    Returns:
        Created MemoryBlock
    """
    mem = currentProgram.getMemory()
    addr_factory = currentProgram.getAddressFactory()
    addr = addr_factory.getAddress("0x%X" % vma)

    if is_bss:
        block = mem.createUninitializedBlock(name, addr, len(data), True)
    else:
        # Convert Python bytes to Java byte array
        jbytes = jarray.array([((b if b < 128 else b - 256)) for b in bytearray(data)], 'b')
        stream = ByteArrayInputStream(jbytes)
        block = mem.createInitializedBlock(name, addr, stream, len(data), monitor, True)

    # Set permissions - overlays contain executable code
    block.setRead(True)
    block.setWrite(True)
    block.setExecute(not is_bss)

    return block

def disassemble_overlay(block):
    """Trigger disassembly on an overlay block."""
    from ghidra.app.cmd.disassemble import DisassembleCommand

    start = block.getStart()
    end = block.getEnd()
    addr_set = currentProgram.getAddressFactory().getAddressSet(start, end)

    cmd = DisassembleCommand(addr_set, None)
    success = cmd.applyTo(currentProgram, monitor)

    if not success:
        return 0

    return cmd.getDisassembledAddressSet().getNumAddresses()

def run():
    elf_path = askFile("Select ELF file", "Open")
    if elf_path is None:
        println("Cancelled")
        return

    println("Reading ELF: " + str(elf_path))
    data = read_file(str(elf_path))

    sections = parse_all_sections(data)
    by_vma = find_overlay_sections(sections)

    println("\nOverlays grouped by VMA:")
    for vma in sorted(by_vma.keys()):
        overlay_nums = [str(num) for num, _, _ in by_vma[vma]]
        println("  0x%08X: overlays %s" % (vma, ", ".join(overlay_nums)))

    # Create overlay blocks
    created_count = 0
    for vma in sorted(by_vma.keys()):
        overlays_at_vma = by_vma[vma]

        for i, (num, code_sec, bss_sec) in enumerate(overlays_at_vma):
            if i == 0:
                # First overlay at this VMA - already in main memory from ELF import
                println("\nSkipping overlay_%d (first at 0x%08X, use existing block)" % (num, vma))
                continue

            # Create code block
            code_name = "overlay_%d" % num
            code_data = data[code_sec['offset']:code_sec['offset'] + code_sec['size']]

            println("\nCreating %s at 0x%08X (0x%X bytes)" % (code_name, vma, len(code_data)))
            try:
                block = create_overlay_block(code_name, vma, code_data)
                println("  Created: %s" % block.getName())
                created_count += 1
            except Exception as e:
                println("  ERROR: %s" % str(e))
                continue

            # Disassemble code block
            try:
                disasm_count = disassemble_overlay(block)
                println("  Disassembled %d addresses" % disasm_count)
            except Exception as e:
                println("  Disassembly error: %s" % str(e))

            # Create BSS block if present
            if bss_sec:
                bss_name = "overlay_%d_bss" % num
                bss_vma = bss_sec['addr']
                bss_size = bss_sec['size']

                println("  Creating %s at 0x%08X (0x%X bytes)" % (bss_name, bss_vma, bss_size))
                try:
                    # BSS is uninitialized, just needs size
                    bss_block = create_overlay_block(bss_name, bss_vma, bytearray(bss_size), is_bss=True)
                    println("    Created: %s" % bss_block.getName())
                except Exception as e:
                    println("    ERROR: %s" % str(e))

    println("\n=== Created %d overlay blocks ===" % created_count)
    println("\n=== Summary ===")
    println("Overlays are now accessible via their overlay address space.")
    println("Navigate using: Go To -> overlay_N::ADDRESS")
    println("Example: overlay_3::801f4a30")

if __name__ == "__main__":
    run()
