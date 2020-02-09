#version 450

layout(location = 0) in vec3 iPos;
layout(location = 1) in vec3 iColor;
layout(location = 2) in vec2 iTexCoords;

layout(location = 0) out vec3 VertColor;
layout(location = 1) out vec2 TexCoords;

layout(binding = 0) uniform Matrices {
    mat4 model;
    mat4 view;
    mat4 projection;
} matrices;

void main() {
    VertColor = iColor;
    TexCoords = iTexCoords;
    gl_Position = matrices.projection * matrices.view * matrices.model * vec4(iPos, 1.0);
}