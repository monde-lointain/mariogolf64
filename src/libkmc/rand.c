extern int f_RandomState;

/*
 * The usual linear congruential generator, except that it takes the
 * result's bits from one past the new state
 */
int rand(void) {
  f_RandomState = f_RandomState * 1103515245 + 12345;
  return ((f_RandomState + 1) >> 16) & 0x7FFF;
}

void srand(unsigned int seed) {
  f_RandomState = seed;
}
