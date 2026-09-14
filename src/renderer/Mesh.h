#pragma once
#include "renderer/Vertex.h"
#include "renderer/VulkanContext.h"
#include <vector>

class Mesh {
public:
    void create(VulkanContext& ctx, const std::vector<Vertex>& v, const std::vector<uint32_t>& i);
    void destroy(VulkanContext& ctx);
    void bind(vk::CommandBuffer cmd) const;
    void draw(vk::CommandBuffer cmd) const;
    uint32_t indexCount() const { return m_indexCount; }

    static Mesh makeCube(VulkanContext& ctx, glm::vec3 color);
    static Mesh makeGround(VulkanContext& ctx, float size, glm::vec3 color);
    static Mesh makeQuad(VulkanContext& ctx, glm::vec3 color);

private:
    VkBuffer m_vbuf = VK_NULL_HANDLE;
    VmaAllocation m_valloc = VK_NULL_HANDLE;
    VkBuffer m_ibuf = VK_NULL_HANDLE;
    VmaAllocation m_ialloc = VK_NULL_HANDLE;
    uint32_t m_indexCount = 0;
};