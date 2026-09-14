#pragma once
#include "renderer/VulkanContext.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

class Texture {
public:
    void createFromPixels(VulkanContext& ctx, uint32_t w, uint32_t h, const uint8_t* rgba);
    void destroy(VulkanContext& ctx);

    vk::ImageView view() const { return m_view; }
    vk::Sampler sampler() const { return m_sampler; }

    // Proceduralne generatory (zwracają RGBA8, rozmiar size×size)
    static std::vector<uint8_t> genGrass(uint32_t size);
    static std::vector<uint8_t> genBricks(uint32_t size);
    static std::vector<uint8_t> genStone(uint32_t size);
    static std::vector<uint8_t> genWood(uint32_t size);
    static std::vector<uint8_t> genLeaves(uint32_t size);
    static std::vector<uint8_t> genMetal(uint32_t size);
    static std::vector<uint8_t> genChecker(uint32_t size, uint32_t squares,
        glm::vec3 a, glm::vec3 b);
    static std::vector<uint8_t> genBulletHole(uint32_t size);

private:
    vk::Image m_image;
    VmaAllocation m_alloc = VK_NULL_HANDLE;
    vk::ImageView m_view;
    vk::Sampler m_sampler;
};