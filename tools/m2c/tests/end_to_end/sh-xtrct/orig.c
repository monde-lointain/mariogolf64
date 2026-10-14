unsigned test(unsigned lhs, unsigned rhs) {
    return (lhs >> 16) | (rhs << 16);
}
