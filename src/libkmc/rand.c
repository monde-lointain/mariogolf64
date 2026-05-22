/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

/*
    int rand(void);
*/
#include "_kmclib.h"
#include <stdlib.h>

/* Upstream declares `static long next;`. Dropped to `extern` because the
   BSS slot is supplied by splat at 0x800FBD30 — keeping `static` would
   place rand.o's local BSS ahead of the splat-emitted `next` and shift
   every downstream BSS symbol (see CLAUDE.md "File-scope `static` blocks
   `/decomp-libupstream`"). Codegen is identical because `next` resolves
   to the same vram either way. */
extern long next;

int rand()
{
/*   long next; */

    next = next * 1103515245 + 12345;
    return ((unsigned int)((next+1)>>16) & RAND_MAX);

    return 0;
}

void srand(seed)
unsigned seed;
{
   next = seed;
}

