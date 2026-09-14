#include "renderer/Mesh.h"
#include <cstring>

static void createBuffer(VulkanContext& ctx, const void* data, VkDeviceSize size,
    VkBufferUsageFlags usage, VkBuffer& outBuf, VmaAllocation& outAlloc) {
    VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info;
    vmaCreateBuffer(ctx.allocator(), &bi, &ai, &outBuf, &outAlloc, &info);
    memcpy(info.pMappedData, data, (size_t)size);
    vmaFlushAllocation(ctx.allocator(), outAlloc, 0, size);
}

void Mesh::create(VulkanContext& ctx, const std::vector<Vertex>& v, const std::vector<uint32_t>& i) {
    m_indexCount = (uint32_t)i.size();
    createBuffer(ctx, v.data(), v.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vbuf, m_valloc);
    createBuffer(ctx, i.data(), i.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_ibuf, m_ialloc);
}

void Mesh::destroy(VulkanContext& ctx) {
    vmaDestroyBuffer(ctx.allocator(), m_vbuf, m_valloc);
    vmaDestroyBuffer(ctx.allocator(), m_ibuf, m_ialloc);
}

void Mesh::bind(vk::CommandBuffer cmd) const {
    vk::Buffer vb(m_vbuf);
    vk::DeviceSize off = 0;
    cmd.bindVertexBuffers(0, 1, &vb, &off);
    cmd.bindIndexBuffer(vk::Buffer(m_ibuf), 0, vk::IndexType::eUint32);
}

void Mesh::draw(vk::CommandBuffer cmd) const {
    cmd.drawIndexed(m_indexCount, 1, 0, 0, 0);
}

Mesh Mesh::makeCube(VulkanContext& ctx, glm::vec3 c) {
    struct Face { glm::vec3 normal; glm::vec3 verts[4]; glm::vec2 uvs[4]; };
    glm::vec2 u0(0, 0), u1(1, 0), u2(1, 1), u3(0, 1);
    Face faces[6] = {
        {{ 0, 0, 1}, {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}}, {u0,u1,u2,u3}},
        {{ 0, 0,-1}, {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}}, {u0,u1,u2,u3}},
        {{ 1, 0, 0}, {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}}, {u0,u1,u2,u3}},
        {{-1, 0, 0}, {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}}, {u0,u1,u2,u3}},
        {{ 0, 1, 0}, {{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1}}, {u0,u1,u2,u3}},
        {{ 0,-1, 0}, {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}}, {u0,u1,u2,u3}}
    };

    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    for (auto& f : faces) {
        uint32_t base = (uint32_t)v.size();
        for (int k = 0; k < 4; ++k)
            v.push_back({ f.verts[k] * 0.5f, f.normal, c, f.uvs[k] });
        idx.insert(idx.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }

    Mesh m;
    m.create(ctx, v, idx);
    return m;
}

Mesh Mesh::makeGround(VulkanContext& ctx, float size, glm::vec3 c) {
    float u = size * 0.5f;
    std::vector<Vertex> v = {
        {{-size, 0, -size}, {0,1,0}, c, { 0,  0 }},
        {{ size, 0, -size}, {0,1,0}, c, { u,  0 }},
        {{ size, 0,  size}, {0,1,0}, c, { u,  u }},
        {{-size, 0,  size}, {0,1,0}, c, { 0,  u }}
    };
    std::vector<uint32_t> idx = { 0,1,2, 0,2,3 };
    Mesh m;
    m.create(ctx, v, idx);
    return m;
}

Mesh Mesh::makeQuad(VulkanContext& ctx, glm::vec3 c) {
    // 1x1 quad na plaszczyznie XY, zwrocony w +Z
    std::vector<Vertex> v = {
        {{-0.5f, -0.5f, 0}, {0,0,1}, c, {0,0}},
        {{ 0.5f, -0.5f, 0}, {0,0,1}, c, {1,0}},
        {{ 0.5f,  0.5f, 0}, {0,0,1}, c, {1,1}},
        {{-0.5f,  0.5f, 0}, {0,0,1}, c, {0,1}}
    };
    std::vector<uint32_t> idx = { 0,1,2, 0,2,3 };
    Mesh m;
    m.create(ctx, v, idx);
    return m;
}