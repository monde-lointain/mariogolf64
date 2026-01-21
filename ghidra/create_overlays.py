# Ghidra script to create overlay address spaces for N64 overlays
# @author Claude
# @category N64
# @runtime Jython

from ghidra.program.model.mem import MemoryBlockType
from java.io import ByteArrayInputStream, File, FileInputStream
import struct
import re

def read_file(path):
    """Read entire file as byte array."""
    f = File(path)
    size = f.length()
    fis = FileInputStream(f)
    data = bytearray(size)
    # Jython: read into Java byte array then convert
    jbytes = jarray.zeros(size, 'b')
    fis.read(jbytes)
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

def run():
    # Placeholder - will be expanded
    println("Ghidra Overlay Creator - Skeleton")

if __name__ == "__main__":
    run()
