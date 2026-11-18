#version 450

// The shape overlay's pixels: a disc or a ring, smoothed over a pixel at
// its edges, with a white label, letters in a 5 by 7 font or an arrow.
// The color isn't premultiplied; the pipeline blends by its alpha.

#define STYLE_DISC 0u
#define STYLE_RING 1u

#define LABEL_NONE 0u
#define LABEL_START 5u
#define LABEL_ARROW_UP 6u
#define LABEL_ARROW_DOWN 7u
#define LABEL_ARROW_LEFT 8u
#define LABEL_ARROW_RIGHT 9u

// The rim's width, in radii
#define RIM 0.08
// How much opaquer the rim and the label are than the fill
#define RIM_OPACITY 1.6
#define LABEL_OPACITY 1.8

#define GLYPH_WIDTH 5
#define GLYPH_HEIGHT 7
#define GLYPH_ADVANCE 6

layout(location = 0) in vec2 inLocal;
layout(location = 1) flat in vec4 inColor;
layout(location = 2) flat in uvec2 inKind;
layout(location = 3) flat in float inPixel;

layout(location = 0) out vec4 outColor;

// The glyphs' rows, top first, with the leftmost column the highest of
// five bits: A, B, Z, R, S and T
const uint GLYPHS[6 * GLYPH_HEIGHT] = uint[](
    0x0Eu, 0x11u, 0x11u, 0x1Fu, 0x11u, 0x11u, 0x11u,
    0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x11u, 0x11u, 0x1Eu,
    0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x1Fu,
    0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x14u, 0x12u, 0x11u,
    0x0Fu, 0x10u, 0x10u, 0x0Eu, 0x01u, 0x01u, 0x1Eu,
    0x1Fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u
);
const uint START_GLYPHS[5] = uint[](4u, 5u, 0u, 3u, 5u);

// How much of a pixel an edge covers, from its signed distance in radii,
// negative inside
float Cover(float distance) {
    return clamp(0.5 - (distance / inPixel), 0.0, 1.0);
}

bool IsLetterSet(uint label, vec2 local) {
    int count = (label == LABEL_START) ? 5 : 1;
    // A letter alone is most of the disc's height; START spans its width
    float cell = (label == LABEL_START) ?
        1.5 / float((count * GLYPH_ADVANCE) - 1) :
        0.9 / float(GLYPH_HEIGHT);
    vec2 origin = -0.5 * cell * vec2(float((count * GLYPH_ADVANCE) - 1), float(GLYPH_HEIGHT));
    ivec2 place = ivec2(floor((local - origin) / cell));
    int index = place.x / GLYPH_ADVANCE;
    int column = place.x - (index * GLYPH_ADVANCE);
    uint glyph;

    if (place.x < 0 || index >= count || column >= GLYPH_WIDTH || place.y < 0 ||
        place.y >= GLYPH_HEIGHT) {
        return false;
    }

    glyph = (label == LABEL_START) ? START_GLYPHS[index] : label - 1u;
    return ((GLYPHS[(glyph * uint(GLYPH_HEIGHT)) + uint(place.y)] >> uint(4 - column)) & 1u) != 0u;
}

// The letters' blocks are smoothed by four samples in each pixel
float CoverLetters(uint label, vec2 local) {
    float quarter = inPixel * 0.25;
    float count = 0.0;

    count += IsLetterSet(label, local + vec2(-quarter, -quarter)) ? 1.0 : 0.0;
    count += IsLetterSet(label, local + vec2(quarter, -quarter)) ? 1.0 : 0.0;
    count += IsLetterSet(label, local + vec2(-quarter, quarter)) ? 1.0 : 0.0;
    count += IsLetterSet(label, local + vec2(quarter, quarter)) ? 1.0 : 0.0;
    return count * 0.25;
}

// A triangle pointing the label's way, its edges' distances turned so
// that it points up the screen
float CoverArrow(uint label, vec2 local) {
    const vec2 apex = vec2(0.0, -0.42);
    // Square to the edge from the apex to the base's right corner
    const vec2 side = normalize(vec2(0.72, -0.42));
    vec2 point = local;
    float distance;

    if (label == LABEL_ARROW_DOWN) {
        point = vec2(local.x, -local.y);
    }
    else if (label == LABEL_ARROW_LEFT) {
        point = vec2(local.y, local.x);
    }
    else if (label == LABEL_ARROW_RIGHT) {
        point = vec2(local.y, -local.x);
    }

    distance = max(point.y - 0.30, dot(vec2(abs(point.x), point.y) - apex, side));
    return Cover(distance);
}

float CoverLabel(uint label, vec2 local) {
    if (label == LABEL_NONE) {
        return 0.0;
    }

    if (label >= LABEL_ARROW_UP) {
        return CoverArrow(label, local);
    }

    return CoverLetters(label, local);
}

void main() {
    float distance = length(inLocal);
    float disc = Cover(distance - 1.0);
    float rim = disc * (1.0 - Cover(distance - (1.0 - RIM)));
    float fill = (inKind.x == STYLE_DISC) ? disc : 0.0;
    float label = (inKind.x == STYLE_DISC) ? disc * CoverLabel(inKind.y, inLocal) : 0.0;
    vec3 color = mix(inColor.rgb, vec3(1.0), rim * 0.35);
    float alpha = max(fill * inColor.a, rim * min(1.0, inColor.a * RIM_OPACITY));

    color = mix(color, vec3(1.0), label);
    alpha = mix(alpha, min(1.0, inColor.a * LABEL_OPACITY), label);
    outColor = vec4(color, alpha);
}
