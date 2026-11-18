/*
 * Tests of the performance meter's digit texture on a host. The game
 * declares the texture as u16 words of packed 4-bit texels, which the port
 * fills from the ROM as the host's values, and which the RDP reads in the
 * N64's byte order; the meter must hand the RDP the ROM's bytes, however
 * many times it draws.
 */

#include <SDL3/SDL.h>
#include <nusys.h>
#include <stdbool.h>
#include <string.h>
#include <ultra64.h>

#include "debug/performance_meter.h"
#include "port/os_host.h"
#include "system/c_language.h"
#include "system/heap.h"
#include "system/rom_arrays.h"
#include "test_support.h"

#define G_SETTIMG_OPCODE 0xFD
#define G_ENDDL_OPCODE 0xDF
// More commands than the meter writes
#define COMMAND_LIMIT 4096
// The digits' 64x8 texels of 4 bits
#define DIGIT_TEXTURE_SIZE (64 * 8 / 2)
// Bytes of the ROM past the digits, which no fill reads
#define ROM_SLACK 16

// A ROM that reaches past the digits, whose bytes differ from their
// neighbors'
static u8 *f_Rom;

static u16 f_Framebuffer[320 * 240];
u16 *nuGfxCfb_ptr = f_Framebuffer;
u32 nuGfxCfbNum = 2;
NUDebTaskPerf *nuDebTaskPerfPtr;
static Gfx *f_StartedList;

// --- Doubles for performance_meter.c's collaborators ---

void nuGfxTaskStart(Gfx *gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag) {
    (void)gfxListSize;
    (void)ucode;
    (void)flag;
    f_StartedList = gfxList_ptr;
}

u32 GetFreeMemorySize(void) {
    return 0x1000;
}

u32 FindLargestFreeMemorySize(void) {
    return 0x800;
}

// The meter's matrices don't matter to its texture
void guMtxIdent(Mtx *m) {
    memset(m, 0, sizeof(*m));
}

void guOrtho(Mtx *m, float l, float r, float b, float t, float n, float f, float scale) {
    memset(m, 0, sizeof(*m));
}

void guLookAt(
    Mtx *m,
    float xEye,
    float yEye,
    float zEye,
    float xAt,
    float yAt,
    float zAt,
    float xUp,
    float yUp,
    float zUp
) {
    memset(m, 0, sizeof(*m));
}

void guScale(Mtx *m, float x, float y, float z) {
    memset(m, 0, sizeof(*m));
}

void guTranslate(Mtx *m, float x, float y, float z) {
    memset(m, 0, sizeof(*m));
}

// --- Scaffolding ---

// The image of the first texture load of the meter's display list, which
// is the digits'
static const u8 *FindDigitTexture(void) {
    int i;

    for (i = 0; f_StartedList != NULL && i < COMMAND_LIMIT; i++) {
        uintptr_t opcode = (f_StartedList[i].words.w0 >> 24) & 0xFF;

        if (opcode == G_SETTIMG_OPCODE) {
            return (const u8 *)f_StartedList[i].words.w1;
        }

        if (opcode == G_ENDDL_OPCODE) {
            break;
        }
    }

    return NULL;
}

// Fills the digits from a ROM of numbered bytes, as the port does at
// start-up
static void FillDigitsFromRom(void) {
    size_t size = digit_texture_ROM_OFFSET + DIGIT_TEXTURE_SIZE + ROM_SLACK;
    size_t i;

    f_Rom = SDL_malloc(size);
    SDL_assert_always(f_Rom != NULL);
    for (i = 0; i < size; i++) {
        f_Rom[i] = (u8)((i * 37) + 11);
    }

    PcInsertCartridge(f_Rom, size);
    osPcFillRomArray(&digit_texture_ROM_ARRAY);
}

// Whether the texture's bytes are the ROM's, and marked as in the
// cartridge's order
static bool IsInRomOrder(const u8 *texture) {
    return SDL_memcmp(texture, &f_Rom[digit_texture_ROM_OFFSET], DIGIT_TEXTURE_SIZE) == 0 &&
        osPcIsRomOrder(&texture[0]) &&
        osPcIsRomOrder(&texture[DIGIT_TEXTURE_SIZE - 1]);
}

// The first draw hands the RDP the digits in the cartridge's order, and a
// second draw leaves them so rather than swapping them back
static void TestDigitsLoadInRomOrder(void) {
    const u8 *texture;
    int draw;

    for (draw = 0; draw < 2; draw++) {
        f_StartedList = NULL;
        DrawMemoryMeter(0, 0);
        texture = FindDigitTexture();
        EXPECT(texture != NULL);
        if (texture == NULL) {
            return;
        }

        EXPECT(IsInRomOrder(texture));
    }
}

int main(void) {
    FillDigitsFromRom();
    TestDigitsLoadInRomOrder();
    SDL_free(f_Rom);
    return ReportTestResults("performance_meter_test");
}
