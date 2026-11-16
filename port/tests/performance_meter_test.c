/*
 * Tests of the performance meter's digit texture on a host. The game
 * declares the texture as u16 words of packed 4-bit texels, which the RDP
 * reads in the N64's byte order; the meter must hand the RDP those bytes,
 * however many times it draws.
 */

#include <nusys.h>
#include <stdbool.h>
#include <string.h>
#include <ultra64.h>

#include "debug/performance_meter.h"
#include "system/c_language.h"
#include "system/heap.h"
#include "test_support.h"

#define G_SETTIMG_OPCODE 0xFD
#define G_ENDDL_OPCODE 0xDF
// More commands than the meter writes
#define COMMAND_LIMIT 4096

// The words the game's source declares, from the same extracted asset that
// the game's build includes
static const u16 f_DeclaredDigits[] = {
#include "assets/debug/performance_meter/digit_texture.ia4.inc.c"
};

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

// Whether the texture's bytes are the declared words, high byte first, and
// marked as in the cartridge's order
static bool IsInRomOrder(const u8 *texture) {
    size_t i;

    for (i = 0; i < ARRAY_COUNT(f_DeclaredDigits); i++) {
        if (texture[i * 2] != (f_DeclaredDigits[i] >> 8) ||
            texture[(i * 2) + 1] != (f_DeclaredDigits[i] & 0xFF)) {
            return false;
        }
    }

    return osPcIsRomOrder(&texture[0]) && osPcIsRomOrder(&texture[sizeof(f_DeclaredDigits) - 1]);
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
    TestDigitsLoadInRomOrder();
    return ReportTestResults("performance_meter_test");
}
