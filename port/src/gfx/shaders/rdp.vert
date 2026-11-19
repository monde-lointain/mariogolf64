#version 450

// The vertices that the interpreter records are in clip space, already
// clipped at the near and far planes, so w is never negative. This divides
// by w itself and hands the GPU a w of 1, as the RDP is handed points on
// the screen: it steps shade linearly across the screen, and texture
// coordinates as their ratios to w along with w's reciprocal, which the
// fragment shader divides by. Clipping such points in clip space,
// lavapipe gave shade no number where a corner lay past two edges and the
// corners' w differed.

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inShade;
layout(location = 3) in uint inMaterial;

// The texture coordinates over w, then w's reciprocal
layout(location = 0) noperspective out vec3 outTexCoord;
layout(location = 1) noperspective out vec4 outShade;
layout(location = 2) flat out uint outMaterial;

void main() {
    float reciprocal = 1.0 / inPosition.w;

    gl_Position = vec4(inPosition.xyz * reciprocal, 1.0);
    outTexCoord = vec3(inTexCoord * reciprocal, reciprocal);
    outShade = inShade;
    outMaterial = inMaterial;
}
