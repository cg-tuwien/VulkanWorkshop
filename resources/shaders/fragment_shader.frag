#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform sampler2D textureSampler;

layout(location = 0) in vec2 inTextureCoordinate;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(texture(textureSampler, inTextureCoordinate).rgb, 1.0);
}