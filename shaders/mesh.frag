#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorld;
layout(location = 3) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 model;
    int  texIndex;
} pc;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;       // <-- musi być!
    mat4 proj;
    vec4 camPos;
} cam;

layout(set = 0, binding = 1) uniform sampler2D textures[16];

void main() {
    vec4 tex = texture(textures[pc.texIndex], fragUV);
    vec3 albedo = tex.rgb * fragColor;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.4, -1.0, 0.3));
    float diff = max(dot(N, -L), 0.0);

    vec3 V = normalize(cam.camPos.xyz - fragWorld);
    vec3 H = normalize(-L + V);
    float spec = pow(max(dot(N, H), 0.0), 48.0);

    vec3 ambient  = albedo * 0.32;
    vec3 diffuse  = albedo * diff * 0.85;
    vec3 specular = vec3(1.0) * spec * 0.20;

    vec3 color = ambient + diffuse + specular;

    float dist = length(cam.camPos.xyz - fragWorld);
    float fog  = clamp((dist - 18.0) / 45.0, 0.0, 1.0);
    color = mix(color, vec3(0.55, 0.68, 0.85), fog);

    outColor = vec4(color, 1.0);
}