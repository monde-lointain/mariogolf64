/*
 * The LZ decoders, which the loaders in hole_loader.c feed a chunk at a time.
 * The original has them at the start of that file's code.
 */

#include "course/lz_decoder.h"

#include <string.h>
#include <ultra64.h>

#include "system/big_endian.h"
#include "system/c_language.h"
#include "system/memory_address.h"

// Shifts the control bits up by count. They pass through the sign bit, as the
// decoders test the top one by its sign, which C leaves undefined to shift
// into or out of as an s32; shifting a u32 gives the same bits.
#ifdef AVOID_UB
#    define ShiftControlBits(controlBits, count) \
        ((controlBits) = (s32)((u32)(controlBits) << (count)))
#else
#    define ShiftControlBits(controlBits, count) ((controlBits) <<= (count))
#endif

#ifdef AVOID_UB
// The decoded words are the bytes of whatever the data holds, which the game
// then reads and stores under its own types, such as a Mtx's s32s. Copying
// them as u16s would make u16 their type, which strict aliasing lets the
// compiler assume those other types don't read, so they are copied as bytes.
#    define CopyLzWord(destination, source) memcpy((destination), (source), sizeof(u16))
#endif

// Decodes the compressed words from stream->input to stream->inputEnd onto
// the end of the output. Returns the output's size in bytes once the stream
// ends. If the chunk runs low first and more is to come, moves the words left
// to just before stream->nextInput, saves its place and returns -1.
s32 DecodeLzChunk(LzStream *stream) {
    u16 *input;
    u16 *output;
    u16 *inputEnd;
    u16 *carriedInput;
    u16 *source;
    s32 controlBits;
    u16 reference;
    u16 *referenceEnd;
    u16 *referenceOutput;
#ifdef AVOID_UB
    s32 literal;
#else
    u16 evenWord;
    u16 oddWord;
#endif
    Boolean hasMoreInput;

    if (stream->flags & LZ_STREAM_FLAG_FIRST) {
        input = stream->input;
        output = stream->outputStart;
        input += LZ_HEADER_WORD_COUNT;
    }
    else {
        input = stream->input;
        output = stream->output;
    }

    inputEnd = stream->inputEnd;
    hasMoreInput = !(stream->flags & LZ_STREAM_FLAG_LAST);
    while (TRUE) {
        if (input >= inputEnd - LZ_LITERAL_RUN_LENGTH) {
            if (hasMoreInput) {
                carriedInput = stream->nextInput;
                while (input < inputEnd) {
#ifdef AVOID_UB
                    carriedInput--;
                    inputEnd--;
                    CopyLzWord(carriedInput, inputEnd);
#else
                    *--carriedInput = *--inputEnd;
#endif
                }

                stream->input = carriedInput;
                stream->output = output;
                return -1;
            }
        }

        controlBits = FromBigEndian16(*input++);
        if (controlBits == 0) {
#ifdef AVOID_UB
            // From the first word, as the original copies them, in case the
            // output trails the input in the same buffer
            for (literal = 0; literal < LZ_LITERAL_RUN_LENGTH; literal++) {
                CopyLzWord(&output[literal], &input[literal]);
            }
#else
            evenWord = input[0];
            oddWord = input[1];
            output[0] = evenWord;
            evenWord = input[2];
            output[1] = oddWord;
            oddWord = input[3];
            output[2] = evenWord;
            evenWord = input[4];
            output[3] = oddWord;
            oddWord = input[5];
            output[4] = evenWord;
            evenWord = input[6];
            output[5] = oddWord;
            oddWord = input[7];
            output[6] = evenWord;
            evenWord = input[8];
            output[7] = oddWord;
            oddWord = input[9];
            output[8] = evenWord;
            evenWord = input[10];
            output[9] = oddWord;
            oddWord = input[11];
            output[10] = evenWord;
            evenWord = input[12];
            output[11] = oddWord;
            oddWord = input[13];
            output[12] = evenWord;
            evenWord = input[14];
            output[13] = oddWord;
            oddWord = input[15];
            output[14] = evenWord;
            output[15] = oddWord;
#endif
            input += LZ_LITERAL_RUN_LENGTH;
            output += LZ_LITERAL_RUN_LENGTH;
            continue;
        }

        ShiftControlBits(controlBits, LZ_CONTROL_BIT_COUNT);
        controlBits |= LZ_CONTROL_END_BIT;
        while (TRUE) {
            while (controlBits >= 0) {
#ifdef AVOID_UB
                CopyLzWord(output, input);
                output++;
                input++;
#else
                *output++ = *input++;
#endif
                ShiftControlBits(controlBits, 1);
            }

            ShiftControlBits(controlBits, 1);
            if (controlBits == 0) {
                break;
            }

            reference = FromBigEndian16(*input++);
            if (reference == 0) {
                return BytesBetween(stream->outputStart, output);
            }

            source = output - (reference >> LZ_REFERENCE_DISTANCE_SHIFT);
            referenceEnd =
                output + ((reference & LZ_REFERENCE_LENGTH_MASK) + LZ_REFERENCE_LENGTH_MIN);
#ifdef AVOID_UB
            // A reference may overlap the words it decodes, so it is copied a
            // word at a time, from its start
            CopyLzWord(output, source);
            source++;
            referenceOutput = output + 1;
            do {
                CopyLzWord(referenceOutput, source);
                referenceOutput++;
                source++;
            }
            while (referenceOutput != referenceEnd);
#else
            *output = *source++;
            referenceOutput = output + 1;
            do {
                *referenceOutput++ = *source++;
            }
            while (referenceOutput != referenceEnd);
#endif
            output = referenceOutput;
        }
    }
}

// Like DecodeLzChunk, but the decoded words are commands that patch the data
// already in the output buffer: a run of literal words, a copy from the same
// or a later word of the buffer, or a copy from the words it overwrote.
s32 DecodePatchedLzChunk(LzStream *stream) {
    u16 patchCount;
    u16 *history;
    u16 historyIndex;
    u16 *output;
    u16 *input;
    u16 *inputEnd;
    u16 *window;
    u16 *carriedInput;
    u16 windowIndex;
    s32 controlBits;
    s32 reference;
    s32 length;
    s32 index;
    u16 word;
    Boolean hasMoreInput;

    auto inline void ApplyPatchCommand(u16 command);

    // Nested, so that it reaches the patch state through the static chain as the
    // original does
    inline void ApplyPatchCommand(u16 command) {
        s16 offset;
        u16 copiedWord;

        patchCount = (command & LZ_PATCH_COUNT_MASK) + 1;
        if ((command & LZ_PATCH_KIND_MASK) != LZ_PATCH_KIND_LITERALS) {
            offset = (s16)(command & LZ_PATCH_KIND_MASK) >> LZ_PATCH_OFFSET_SHIFT;
            if (offset < 0) {
                while (patchCount != 0) {
                    copiedWord = history[(historyIndex + offset) & LZ_HISTORY_MASK];
#ifdef AVOID_UB
                    CopyLzWord(&history[historyIndex], output);
                    historyIndex++;
                    historyIndex &= LZ_HISTORY_MASK;
                    CopyLzWord(output, &copiedWord);
                    output++;
#else
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = copiedWord;
#endif
                    patchCount--;
                }
            }
            else {
                while (patchCount != 0) {
#ifdef AVOID_UB
                    // Through copiedWord, as an offset of 0 copies the word
                    // onto itself, which memcpy can't
                    CopyLzWord(&history[historyIndex], output);
                    historyIndex++;
                    historyIndex &= LZ_HISTORY_MASK;
                    CopyLzWord(&copiedWord, &output[offset]);
                    CopyLzWord(output, &copiedWord);
#else
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output = output[offset];
#endif
                    output++;
                    patchCount--;
                }
            }
        }
    }

    inputEnd = stream->inputEnd;
    hasMoreInput = !(stream->flags & LZ_STREAM_FLAG_LAST);
    if (stream->flags & LZ_STREAM_FLAG_FIRST) {
        input = stream->input;
        window = stream->window;
        windowIndex = 0;
        controlBits = LZ_SPENT_CONTROL_BITS;
        historyIndex = 0;
        patchCount = 0;
        input += LZ_HEADER_WORD_COUNT;
        output = stream->outputStart;
        history = stream->history;
        // The first chunk enters the decode loop past the refill, as its control
        // bits are already set; the jump is there to match. Skipping the refill
        // on a first-call flag gives 77 words differing, 11 in shape, and the
        // refill written before the loop and again at its end 150, 48 in shape
        goto DECODE;
    }

    output = stream->output;
    input = stream->input;
    window = stream->window;
    windowIndex = stream->windowIndex;
    historyIndex = stream->historyIndex;
    patchCount = stream->patchCount;
    history = stream->history;
    while (TRUE) {
        if (input >= inputEnd - LZ_LITERAL_RUN_LENGTH) {
            if (hasMoreInput) {
                carriedInput = stream->nextInput;
                while (input < inputEnd) {
                    *--carriedInput = *--inputEnd;
                }

                stream->input = carriedInput;
                stream->windowIndex = windowIndex;
                stream->output = output;
                stream->patchCount = patchCount;
                stream->historyIndex = historyIndex;
                return -1;
            }
        }

        controlBits = FromBigEndian16(*input++);
        ShiftControlBits(controlBits, LZ_CONTROL_BIT_COUNT);
        controlBits |= LZ_CONTROL_END_BIT;

DECODE:
        while (TRUE) {
            while (controlBits >= 0) {
                word = *input++;
                window[windowIndex++] = word;
                windowIndex &= LZ_WINDOW_MASK;
                if (patchCount == 0) {
                    ApplyPatchCommand(FromBigEndian16(word));
                }
                else {
#ifdef AVOID_UB
                    CopyLzWord(&history[historyIndex], output);
                    historyIndex++;
                    historyIndex &= LZ_HISTORY_MASK;
                    CopyLzWord(output, &word);
                    output++;
#else
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = word;
#endif
                    patchCount--;
                }

                ShiftControlBits(controlBits, 1);
            }

            ShiftControlBits(controlBits, 1);
            if (controlBits == 0) {
                break;
            }

            reference = FromBigEndian16(*input++);

            // The u16 casts are redundant, but they are needed to match: without
            // either or both, 5 words differ (1 to 3 in shape); with no casts, a
            // u32 reference gives 5 and a u16 one 92
            if ((u16)reference == 0) {
                return BytesBetween(stream->outputStart, output);
            }

            length = reference & 0x1F;
            reference = (u16)reference >> LZ_REFERENCE_DISTANCE_SHIFT;
            length++;
            while (TRUE) {
                index = windowIndex;
                windowIndex = (windowIndex + 1) & LZ_WINDOW_MASK;
                word = window[(index - reference) & LZ_WINDOW_MASK];
                window[index] = word;
                if (patchCount == 0) {
                    ApplyPatchCommand(FromBigEndian16(word));
                }
                else {
#ifdef AVOID_UB
                    CopyLzWord(&history[historyIndex], output);
                    historyIndex++;
                    historyIndex &= LZ_HISTORY_MASK;
                    CopyLzWord(output, &word);
                    output++;
#else
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = word;
#endif
                    patchCount--;
                }

                if (length == 0) {
                    break;
                }

                length--;
            }
        }
    }
}
