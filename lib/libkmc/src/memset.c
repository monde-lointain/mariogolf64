typedef unsigned int size_t;

void* memset(void* dst, int value, size_t length) {
  unsigned char* byte;
  size_t wordCount;

  if (length != 0) {
    /* Repeats the byte through a word, so whole words can be stored */
    value &= 0xFF;
    value |= value << 8;
    value |= value << 16;
    byte = dst;
    if ((unsigned int)byte & 1) {
      *byte++ = value;
      length--;
    }
    if (length >= 2 && ((unsigned int)byte & 2)) {
      *(unsigned short*)byte = value;
      byte += 2;
      length -= 2;
    }
    wordCount = length >> 2;
    while (wordCount-- != 0) {
      *(unsigned int*)byte = value;
      byte += 4;
    }
    if (length & 2) {
      *(unsigned short*)byte = value;
      byte += 2;
    }
    if (length & 1) {
      *byte = value;
    }
  }
  return dst;
}

/* memset with the older argument order */
void setmem(void* dst, size_t length, int value) {
  memset(dst, value, length);
}
