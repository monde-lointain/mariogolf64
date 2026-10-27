extern int sRandomState;

/*
 * The usual linear congruential generator, except that it takes the
 * result's bits from one past the new state
 */
int rand(void) {
  sRandomState = sRandomState * 1103515245 + 12345;
  return ((sRandomState + 1) >> 16) & 0x7FFF;
}

void srand(unsigned int seed) { sRandomState = seed; }
