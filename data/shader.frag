#version 450

layout(location = 0) in vec3 VertColor;
layout(location = 1) in vec2 TexCoords;

layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = texture(tex_sampler, TexCoords);
}