#version 450

// The vertices that the interpreter records: already in clip space, so this
// only hands them on. The RDP steps shade linearly across the screen, so it
// goes without perspective; texture coordinates keep it.

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inShade;
layout(location = 3) in uint inMaterial;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) noperspective out vec4 outShade;
layout(location = 2) flat out uint outMaterial;

void main() {
    gl_Position = inPosition;
    outTexCoord = inTexCoord;
    outShade = inShade;
    outMaterial = inMaterial;
}
