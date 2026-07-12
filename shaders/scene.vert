#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 color;

layout(push_constant) uniform Push
{
    mat4 transform;
    float time;
} pushData;


void main()
{
    gl_Position =
        pushData.transform *
        vec4(inPosition, 0.0, 1.0);

    color = inColor;
}