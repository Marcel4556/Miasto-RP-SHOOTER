#version 450
layout(location = 0) in vec3 fragDir;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main() {
    vec3 dir = normalize(fragDir);
    vec3 color = texture(skybox, dir).rgb;
    outColor = vec4(color, 1.0);
}