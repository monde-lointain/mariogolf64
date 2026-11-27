#version 450

// A triangle that covers the viewport, made from the vertex's index alone,
// with no vertex buffer: the pixels written to a multisampled target, which
// SDL_GPU can't blit to.

void main() {
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));

    gl_Position = vec4((corner * 2.0) - 1.0, 0.0, 1.0);
}
