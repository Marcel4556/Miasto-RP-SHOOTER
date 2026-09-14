#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorld;
layout(location = 3) out vec2 fragUV;

layout(push_constant) uniform Push {
    mat4 model;
    int  texIndex;
} pc;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
    vec4 camPos;
} cam;

void main() {
    vec4 world = pc.model * vec4(inPos, 1.0);
    fragWorld  = world.xyz;
    fragNormal = normalize(mat3(pc.model) * inNormal);
    fragColor  = inColor;
    fragUV     = inUV;
    gl_Position = cam.proj * cam.view * world;
}