#version 450

// The shape overlay's vertices: each shape is an instance, a square of two
// triangles around its circle, made from the shapes' uniform block alone,
// with no vertex buffer.

#define MAX_SHAPES 16

struct Shape {
    vec4 circle; // Center x and y and radius, in the target's pixels
    vec4 color;
    uvec4 kind; // Style and label
};

layout(set = 1, binding = 0, std140) uniform Shapes {
    vec4 targetSize; // Width and height, in pixels
    Shape shapes[MAX_SHAPES];
};

layout(location = 0) out vec2 outLocal; // From the center, in radii
layout(location = 1) flat out vec4 outColor;
layout(location = 2) flat out uvec2 outKind;
layout(location = 3) flat out float outPixel; // A pixel, in radii

const vec2 CORNERS[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

void main() {
    Shape shape = shapes[gl_InstanceIndex];
    float radius = shape.circle.z;
    // Two pixels past the circle, for its smoothed edge
    vec2 local = CORNERS[gl_VertexIndex] * (1.0 + (2.0 / radius));
    vec2 pixel = shape.circle.xy + (local * radius);

    // The target's pixels run down from its top left; its NDC runs up
    gl_Position = vec4(
        ((pixel.x / targetSize.x) * 2.0) - 1.0,
        1.0 - ((pixel.y / targetSize.y) * 2.0),
        0.0,
        1.0
    );
    outLocal = local;
    outColor = shape.color;
    outKind = shape.kind.xy;
    outPixel = 1.0 / radius;
}
