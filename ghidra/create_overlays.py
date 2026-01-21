# Ghidra script to create overlay address spaces for N64 overlays
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

def run():
    elf_path = askFile("Select ELF file", "Open")
    if elf_path is None:
        println("Cancelled")
        return

    println("Reading ELF: " + str(elf_path))
    data = read_file(str(elf_path))

    sections = parse_all_sections(data)
    println("Found %d sections" % len(sections))

    # Print overlay sections
    for sec in sections:
        if sec['name'].startswith('.overlay'):
            println("  %s: addr=0x%08X size=0x%X offset=0x%X" % (
                sec['name'], sec['addr'], sec['size'], sec['offset']))

if __name__ == "__main__":
    run()
