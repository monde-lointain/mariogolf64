/*
 * The interpreter's two halves and what passes between them. The RSP half
 * walks the display lists, transforms and lights vertices and assembles
 * triangles; the RDP half keeps the rasterizer's state, loads textures and
 * records each primitive for the GPU.
 */

#ifndef PORT_GFX_RCP_H
#define PORT_GFX_RCP_H

#include <stdbool.h>
#include <stdint.h>

#include "frame.h"

// A vertex as the RSP leaves it for the RDP
typedef struct RspVertex {
    // Clip space, with the viewport folded in: x and y in pixels times w,
    // z in [0, 1] times w
    float screen[4];
    float texCoord[2]; // In texels
    uint8_t color[4];
    uint8_t clipFlags; // CLIP_* for the planes that the vertex is outside
} RspVertex;

#define CLIP_LEFT 0x01
#define CLIP_RIGHT 0x02
#define CLIP_TOP 0x04
#define CLIP_BOTTOM 0x08
#define CLIP_NEAR 0x10
#define CLIP_FAR 0x20

// The RSP's state that the RDP's primitives depend on
typedef struct RspDrawState {
    uint32_t geometryMode;
    int textureTile; // The tile that gSPTexture names
    bool isTextureOn;
} RspDrawState;

// Where an address that the display lists hold leads in the host's memory,
// through the segment table. The display lists hold host pointers, or N64
// addresses where they came from ROM.
typedef struct RcpMemory {
    uintptr_t segments[16];
} RcpMemory;

const void *ResolveRcpAddress(const RcpMemory *memory, uintptr_t address);

void InitRdp(void);
// Sends what the frame holds and releases the GPU's targets, before the
// backend shuts down
void ShutdownRdp(void);
void ResetRdpForTask(void);

// Runs one RDP command, other than the texture rectangles, which need the
// two words that follow them in the display list
void RunRdpCommand(uint32_t w0, uintptr_t w1, const RcpMemory *memory);

void DrawRdpTriangle(
    const RspVertex *first,
    const RspVertex *second,
    const RspVertex *third,
    const RspDrawState *state
);
void DrawRdpTextureRectangle(
    uint32_t w0,
    uint32_t w1,
    uint32_t texCoords,
    uint32_t texSteps,
    bool isFlipped
);

// Marks the material for rebuilding after the RSP changes what it depends
// on
void InvalidateRdpMaterial(void);

// Sends what the frame holds to the GPU and starts another
void FlushRdpFrame(void);

// Sends the frame and brings the memory of the color images in a range up
// to date, for the CPU or a texture load to read it
void SyncRdpMemory(const void *start, uintptr_t size);

#endif
