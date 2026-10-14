unsigned test(unsigned value) {
    return (value & 0xFFFF0000U) | ((value & 0xFFU) << 8) | ((value >> 8) & 0xFFU);
}
