#version 450

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push
{
    mat4 transform;
    float time;
} pushData;

void main()
{
    float wave =
        sin(pushData.time * 3.0) * 0.5 + 0.5;

    vec3 c =
        mix(
            color,
            color.bgr,
            wave);

    outColor = vec4(c,1.0);
}