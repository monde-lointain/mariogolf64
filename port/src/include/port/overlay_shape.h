/*
 * The shapes that the shape overlay draws over the presented screen, such
 * as the touch controls: discs and rings, each with an optional label.
 */

#ifndef PORT_OVERLAY_SHAPE_H
#define PORT_OVERLAY_SHAPE_H

#include <stdint.h>

// The most shapes that one overlay draws
#define OVERLAY_MAX_SHAPES 16

typedef enum OverlayStyle {
    OVERLAY_STYLE_DISC, // Filled, with a brighter rim
    OVERLAY_STYLE_RING // The rim alone
} OverlayStyle;

typedef enum OverlayLabel {
    OVERLAY_LABEL_NONE,
    OVERLAY_LABEL_A,
    OVERLAY_LABEL_B,
    OVERLAY_LABEL_Z,
    OVERLAY_LABEL_R,
    OVERLAY_LABEL_START,
    OVERLAY_LABEL_ARROW_UP,
    OVERLAY_LABEL_ARROW_DOWN,
    OVERLAY_LABEL_ARROW_LEFT,
    OVERLAY_LABEL_ARROW_RIGHT
} OverlayLabel;

// A shape's place and size are in the swapchain's pixels, from its top
// left. Its color is RGBA8 with red lowest, and not premultiplied; its
// label is drawn white, at the shape's alpha.
typedef struct OverlayShape {
    float x;
    float y;
    float radius;
    uint32_t color;
    uint8_t style; // An OverlayStyle
    uint8_t label; // An OverlayLabel
} OverlayShape;

#endif
