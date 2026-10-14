u32 test(u32 value) {
    return (u32) ((u32) (value & 0xFFFF0000) | (u32) ((value & 0xFF) << 8)) | (u32) ((value >> 8) & 0xFF);
}
