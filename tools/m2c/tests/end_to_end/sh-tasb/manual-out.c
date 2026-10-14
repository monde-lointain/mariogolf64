s32 test(u8 *p) {
    if (!M2C_TAS_B(p)) {
        return 0;
    }
    return 1;
}
