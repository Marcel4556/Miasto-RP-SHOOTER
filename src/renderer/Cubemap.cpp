#include "renderer/Cubemap.h"
#include <cstring>
#include <cmath>
#include <stdexcept>

// Kolor nieba w danym kierunku (gradient + słońce)
static glm::vec3 skyColor(glm::vec3 dir) {
    dir = glm::normalize(dir);
    float t = dir.y;  // -1 (dół) do 1 (góra)

    glm::vec3 zenith(0.10f, 0.32f, 0.72f);
    glm::vec3 mid(0.42f, 0.62f, 0.88f);
    glm::vec3 horizon(0.68f, 0.82f, 0.95f);
    glm::vec3 nadir(0.25f, 0.28f, 0.35f);

    glm::vec3 color;
    if (t >= 0.5f) {
        color = glm::mix(mid, zenith, (t - 0.5f) * 2.0f);
    }
    else if (t >= 0.0f) {
        color = glm::mix(horizon, mid, t * 2.0f);
    }
    else {
        color = glm::mix(horizon, nadir, -t);
    }

    // Słońce – zgrane z kierunkiem światła w mesh.frag
    glm::vec3 sunDir = glm::normalize(glm::vec3(-0.4f, 1.0f, -0.3f));
    float sunDot = glm::max(glm::dot(dir, sunDir), 0.0f);
    float sun = std::pow(sunDot, 256.0f);        // mały jasny punkt
    float glow = std::pow(sunDot, 16.0f) * 0.35f; // miękka poświata
    color += glm::vec3(1.0f, 0.95f, 0.75f) * (sun * 3.0f + glow);

    return color;
}

void Cubemap::createProcedural(VulkanContext& ctx, uint32_t size) {
    const uint32_t faceSize = size * size * 4;
    const uint32_t totalSize = 6 * faceSize;
    std::vector<uint8_t> data(totalSize);

    for (int face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                float u = 2.0f * x / (float)(size - 1) - 1.0f;
                float v = 2.0f * y / (float)(size - 1) - 1.0f;

                glm::vec3 dir;
                switch (face) {
                case 0: dir = { 1, -v, -u }; break; // +X
                case 1: dir = { -1, -v,  u }; break; // -X
                case 2: dir = { u,  1,  v }; break; // +Y
                case 3: dir = { u, -1, -v }; break; // -Y
                case 4: dir = { u, -v,  1 }; break; // +Z
                case 5: dir = { -u, -v, -1 }; break; // -Z
                }

                glm::vec3 c = skyColor(dir);
                size_t i = face * faceSize + (y * size + x) * 4;
                data[i + 0] = (uint8_t)(glm::clamp(c.r, 0.0f, 1.0f) * 255);
                data[i + 1] = (uint8_t)(glm::clamp(c.g, 0.0f, 1.0f) * 255);
                data[i + 2] = (uint8_t)(glm::clamp(c.b, 0.0f, 1.0f) * 255);
                data[i + 3] = 255;
            }
        }
    }

    // Staging
    VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = totalSize;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging;
    VmaAllocation stagingAlloc;
    VmaAllocationInfo stagingInfo;
    vmaCreateBuffer(ctx.allocator(), &bi, &ai, &staging, &stagingAlloc, &stagingInfo);
    memcpy(stagingInfo.pMappedData, data.data(), totalSize);

    // Cubemap image
    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_SRGB;
    ici.extent = { size, size, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 6;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo iai{};
    iai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VkImage rawImage;
    vmaCreateImage(ctx.allocator(), &ici, &iai, &rawImage, &m_alloc, nullptr);
    m_image = vk::Image(rawImage);

    // Copy + transitiony
    ctx.submitImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 toDst;
        toDst.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        toDst.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
        toDst.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        toDst.oldLayout = vk::ImageLayout::eUndefined;
        toDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.image = m_image;
        toDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 };
        cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toDst));

        std::vector<vk::BufferImageCopy> regions;
        for (uint32_t face = 0; face < 6; ++face) {
            vk::BufferImageCopy r(
                face * faceSize,
                0, 0,
                { vk::ImageAspectFlagBits::eColor, 0, face, 1 },
                { 0, 0, 0 },
                { size, size, 1 });
            regions.push_back(r);
        }
        cmd.copyBufferToImage(staging, m_image,
            vk::ImageLayout::eTransferDstOptimal, regions);

        vk::ImageMemoryBarrier2 toRead;
        toRead.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        toRead.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        toRead.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        toRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        toRead.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        toRead.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toRead.image = m_image;
        toRead.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 };
        cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toRead));
        });

    vmaDestroyBuffer(ctx.allocator(), staging, stagingAlloc);

    // Image view – cubemap
    vk::ImageViewCreateInfo vi({}, m_image, vk::ImageViewType::eCube,
        vk::Format::eR8G8B8A8Srgb, {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 });
    m_view = ctx.device().createImageView(vi);

    // Sampler – clamp to edge (dla cubemapy)
    vk::SamplerCreateInfo si({}, vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways);
    m_sampler = ctx.device().createSampler(si);
}

void Cubemap::destroy(VulkanContext& ctx) {
    if (m_sampler) ctx.device().destroySampler(m_sampler);
    if (m_view)    ctx.device().destroyImageView(m_view);
    if (m_image)   vmaDestroyImage(ctx.allocator(), m_image, m_alloc);
}