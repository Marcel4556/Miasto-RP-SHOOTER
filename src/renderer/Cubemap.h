#pragma once
#include "renderer/VulkanContext.h"
#include <cstdint>
#include <glm/glm.hpp>

class Cubemap {
public:
    void createProcedural(VulkanContext& ctx, uint32_t size);
    void destroy(VulkanContext& ctx);

    vk::ImageView view() const { return m_view; }
    vk::Sampler sampler() const { return m_sampler; }

private:
    vk::Image m_image;
    VmaAllocation m_alloc = VK_NULL_HANDLE;
    vk::ImageView m_view;
    vk::Sampler m_sampler;
};