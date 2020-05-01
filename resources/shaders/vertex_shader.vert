#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTextureCoordinate;

layout(location = 0) out vec2 outTextureCoordinate;

void main() {
    outTextureCoordinate = inTextureCoordinate;
    gl_Position = ubo.projMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPosition, 1.0);
}