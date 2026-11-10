/*
 * Finds the matching blocks of two sequences as Python's
 * difflib.SequenceMatcher(None, a, b, autojunk=False) does, for fncompare.
 *
 * difflib's search for the longest match visits each pair of equal entries
 * in the window, and the instructions' operations, of a few dozen kinds,
 * make millions of such pairs in a routine of thousands of instructions.
 * Scores follow difflib's choices, so the blocks must be the same ones,
 * ties broken alike, and not merely as many.
 *
 * Build: cc -O2 -shared -fPIC -o match_blocks.so match_blocks.c
 */

#include <stdint.h>
#include <stdlib.h>

/* A run of equal entries ending at a b index, as a row of a found it. It
 * continues only into the next row, so older runs need no clearing. */
typedef struct {
    uint32_t row;
    uint32_t length;
} Run;

typedef struct {
    int32_t a_start;
    int32_t b_start;
    int32_t size;
} Block;

typedef struct {
    int32_t a_low;
    int32_t a_high;
    int32_t b_low;
    int32_t b_high;
} Window;

typedef struct {
    const int32_t *a;
    const int32_t *b;
    /* The indices in b of each code, in order: those of code c are
     * b_indices[code_starts[c]] up to b_indices[code_starts[c + 1]] */
    int32_t *code_starts;
    int32_t *b_indices;
    /* The run ending at each b index, offset by one so that index 0
     * stands before b */
    Run *runs;
    /* The number of the row of a being matched, which no earlier row of
     * any search shares */
    uint32_t row;
} Matcher;

/* Returns the first of INDICES[first..end) that is at least VALUE. */
static int32_t find_first_at_least(
    const int32_t *indices,
    int32_t first,
    int32_t end,
    int32_t value
) {
    while (first < end) {
        int32_t middle = first + (end - first) / 2;

        if (indices[middle] < value) {
            first = middle + 1;
        }
        else {
            end = middle;
        }
    }

    return first;
}

/* Returns the longest block in the window, the earliest in a and then in b
 * among the longest. */
static Block find_longest_match(Matcher *matcher, Window window) {
    Block best = {window.a_low, window.b_low, 0};
    int32_t a_index;

    /* Skip a row, so that no run of the last search continues into this */
    matcher->row++;
    for (a_index = window.a_low; a_index < window.a_high; a_index++) {
        int32_t code = matcher->a[a_index];
        int32_t first = find_first_at_least(
            matcher->b_indices,
            matcher->code_starts[code],
            matcher->code_starts[code + 1],
            window.b_low
        );
        int32_t end = find_first_at_least(
            matcher->b_indices,
            first,
            matcher->code_starts[code + 1],
            window.b_high
        );
        int32_t row_b = 0;
        uint32_t row_size = 0;
        int32_t position;

        matcher->row++;
        /* Backwards, each run reads the last row's run before it, which this
         * row hasn't yet replaced, and the earliest of the longest in the row
         * is kept */
        for (position = end - 1; position >= first; position--) {
            int32_t b_index = matcher->b_indices[position];
            Run before = matcher->runs[b_index];
            uint32_t size = (before.row + 1 == matcher->row) ? before.length + 1 : 1;

            matcher->runs[b_index + 1].row = matcher->row;
            matcher->runs[b_index + 1].length = size;
            if (size >= row_size) {
                row_b = b_index;
                row_size = size;
            }
        }

        if ((int32_t)row_size > best.size) {
            best.a_start = a_index + 1 - (int32_t)row_size;
            best.b_start = row_b + 1 - (int32_t)row_size;
            best.size = (int32_t)row_size;
        }
    }

    /* Without junk the run found is already as long as it can be */
    return best;
}

static int compare_blocks(const void *left, const void *right) {
    const Block *first = left;
    const Block *second = right;

    if (first->a_start != second->a_start) {
        return (first->a_start < second->a_start) ? -1 : 1;
    }

    if (first->b_start != second->b_start) {
        return (first->b_start < second->b_start) ? -1 : 1;
    }

    return (first->size < second->size) ? -1 : (first->size > second->size);
}

/*
 * Writes the matching blocks of A and B, whose entries are codes below
 * CODE_COUNT, to BLOCKS as (a start, b start, size) triples, in order and
 * with adjacent ones joined, but without the empty block past both. BLOCKS
 * holds at least the shorter sequence's length plus one triples. Returns
 * how many it wrote, or -1 when memory runs out.
 */
int32_t match_blocks(
    const int32_t *a,
    int32_t a_length,
    const int32_t *b,
    int32_t b_length,
    int32_t code_count,
    int32_t *blocks
) {
    Matcher matcher = {a, b, NULL, NULL, NULL, 0};
    Block *found = (Block *)blocks;
    Window *queue = NULL;
    int32_t found_count = 0;
    int32_t queued = 0;
    int32_t joined = 0;
    int32_t index;

    matcher.code_starts = calloc((size_t)code_count + 1, sizeof(int32_t));
    matcher.b_indices = malloc(((size_t)b_length + 1) * sizeof(int32_t));
    matcher.runs = calloc((size_t)b_length + 1, sizeof(Run));
    queue = malloc(((size_t)(a_length < b_length ? a_length : b_length) + 2) * sizeof(Window));
    if (matcher.code_starts == NULL ||
        matcher.b_indices == NULL ||
        matcher.runs == NULL ||
        queue == NULL) {
        found_count = -1;
        goto done;
    }

    /* Count each code's entries in b, then place their indices in order */
    for (index = 0; index < b_length; index++) {
        matcher.code_starts[b[index] + 1]++;
    }

    for (index = 0; index < code_count; index++) {
        matcher.code_starts[index + 1] += matcher.code_starts[index];
    }

    for (index = 0; index < b_length; index++) {
        matcher.b_indices[matcher.code_starts[b[index]]++] = index;
    }

    for (index = code_count; index > 0; index--) {
        matcher.code_starts[index] = matcher.code_starts[index - 1];
    }

    matcher.code_starts[0] = 0;

    queue[queued++] = (Window) {0, a_length, 0, b_length};
    while (queued > 0) {
        Window window = queue[--queued];
        Block block = find_longest_match(&matcher, window);

        if (block.size == 0) {
            continue;
        }

        found[found_count++] = block;
        if (window.a_low < block.a_start && window.b_low < block.b_start) {
            queue[queued++] = (Window) {window.a_low, block.a_start, window.b_low, block.b_start};
        }

        if (block.a_start + block.size < window.a_high &&
            block.b_start + block.size < window.b_high) {
            queue[queued++] = (Window) {
                block.a_start + block.size,
                window.a_high,
                block.b_start + block.size,
                window.b_high
            };
        }
    }

    qsort(found, (size_t)found_count, sizeof(Block), compare_blocks);
    for (index = 0; index < found_count; index++) {
        Block *last = (joined > 0) ? &found[joined - 1] : NULL;

        if (last != NULL &&
            last->a_start + last->size == found[index].a_start &&
            last->b_start + last->size == found[index].b_start) {
            last->size += found[index].size;
        }
        else {
            found[joined++] = found[index];
        }
    }

    found_count = joined;

done:
    free(matcher.code_starts);
    free(matcher.b_indices);
    free(matcher.runs);
    free(queue);
    return found_count;
}
