#version 450

// The N64's pixels, from the staging texture, written to a scaled target:
// each of the target's pixels takes the one of the N64's that it lies in,
// so that each of those becomes a square of scale by scale, and every
// sample of a pixel takes its color.

layout(set = 2, binding = 0) uniform sampler2D staging;

layout(set = 3, binding = 0, std140) uniform Placement {
    // The top of the rows written, in the target's pixels, and the scale
    uvec4 placement;
};

layout(location = 0) out vec4 outColor;

void main() {
    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint scale = placement.y;

    outColor = texelFetch(staging, ivec2(pixel.x / scale, (pixel.y - placement.x) / scale), 0);
}
