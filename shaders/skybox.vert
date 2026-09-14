#version 450
layout(location = 0) out vec3 fragDir;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
    vec4 camPos;
} cam;

// 36 wierzchołków sześcianu (2 trójkąty na każdą ze 6 ścian)
vec3 cubeVerts[36] = vec3[](
    // +Z
    vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1, 1, 1),
    vec3(-1,-1, 1), vec3( 1, 1, 1), vec3(-1, 1, 1),
    // -Z
    vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1, 1,-1),
    vec3( 1,-1,-1), vec3(-1, 1,-1), vec3( 1, 1,-1),
    // +X
    vec3( 1,-1, 1), vec3( 1,-1,-1), vec3( 1, 1,-1),
    vec3( 1,-1, 1), vec3( 1, 1,-1), vec3( 1, 1, 1),
    // -X
    vec3(-1,-1,-1), vec3(-1,-1, 1), vec3(-1, 1, 1),
    vec3(-1,-1,-1), vec3(-1, 1, 1), vec3(-1, 1,-1),
    // +Y
    vec3(-1, 1, 1), vec3( 1, 1, 1), vec3( 1, 1,-1),
    vec3(-1, 1, 1), vec3( 1, 1,-1), vec3(-1, 1,-1),
    // -Y
    vec3(-1,-1,-1), vec3( 1,-1,-1), vec3( 1,-1, 1),
    vec3(-1,-1,-1), vec3( 1,-1, 1), vec3(-1,-1, 1)
);

void main() {
    vec3 pos = cubeVerts[gl_VertexIndex];
    fragDir = pos;

    // Usuń translację z view matrix – kamera zawsze w środku sześcianu
    mat4 viewNoTrans = mat4(mat3(cam.view));

    vec4 clipPos = cam.proj * viewNoTrans * vec4(pos, 1.0);
    // Ustaw głębokość na 1.0 (daleko) – skybox zawsze w tle
    gl_Position = clipPos.xyww;
}