/*
 * Compares a word at a time where it can. The word difference wraps, so
 * strings whose first differing bytes are more than 0x7F apart can compare
 * with the wrong sign there: "\x01" against "\xFF" gives a positive result
 */
int strcmp(const char *left, const char *right) {
    const unsigned char *leftByte = (const unsigned char *)left;
    const unsigned char *rightByte = (const unsigned char *)right;
    unsigned int leftWord;
    unsigned int rightWord;
    int character;
    int difference;

    /* Strings that can never be word-aligned together go byte by byte */
    if (((unsigned int)leftByte - (unsigned int)rightByte) & 3) {
        for (;;) {
            character = *leftByte++;
            difference = character - *rightByte++;
            if (difference != 0 | character == 0) {
                return difference;
            }
        }
    }
    while ((unsigned int)leftByte & 3) {
        character = *leftByte++;
        difference = character - *rightByte++;
        if (difference != 0 | character == 0) {
            return difference;
        }
    }
    for (;;) {
        leftWord = *(const unsigned int *)leftByte;
        leftByte += 4;
        rightWord = *(const unsigned int *)rightByte;
        rightByte += 4;
        difference = leftWord - rightWord;
        if (difference != 0) {
            /*
             * Only the bytes up to the left string's end take part. The mask
             * shares difference's variable, as the original's registers show
             */
            if (!(leftWord & 0xFF000000)) {
                difference = 0xFF000000;
            } else if (!(leftWord & 0xFF0000)) {
                difference = 0xFFFF0000;
            } else if (!(leftWord & 0xFF00)) {
                difference = 0xFFFFFF00;
            } else {
                return difference;
            }
            return (leftWord & difference) - (rightWord & difference);
        }
        if (!(leftWord << 24) || !(leftWord & 0xFF00) || !(leftWord & 0xFF0000)
            || !(leftWord & 0xFF000000)) {
            return difference;
        }
    }
}
