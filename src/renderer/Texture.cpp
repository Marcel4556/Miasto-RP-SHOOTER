#include "renderer/Texture.h"
#include <cstring>
#include <cmath>

// ============================================================
//  VALUE NOISE (do generowania proceduralnych tekstur)
// ============================================================
static float hash2(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff)
        / 1073741824.0f;
}

static float smoothNoise(float x, float y) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    float a = hash2(ix, iy);
    float b = hash2(ix + 1, iy);
    float c = hash2(ix, iy + 1);
    float d = hash2(ix + 1, iy + 1);
    float u = fx * fx * (3 - 2 * fx);
    float v = fy * fy * (3 - 2 * fy);
    return glm::mix(glm::mix(a, b, u), glm::mix(c, d, u), v);
}

static float fbm(float x, float y, int octaves = 4) {
    float v = 0, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        v += smoothNoise(x * freq, y * freq) * amp;
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return v * 0.5f + 0.5f;
}

// ============================================================
//  UPLOAD DO GPU
// ============================================================
void Texture::createFromPixels(VulkanContext& ctx, uint32_t w, uint32_t h, const uint8_t* rgba) {
    vk::DeviceSize size = (vk::DeviceSize)w * h * 4;

    // --- Staging buffer ---
    VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging; VmaAllocation stagingAlloc;
    VmaAllocationInfo info;
    vmaCreateBuffer(ctx.allocator(), &bi, &ai, &staging, &stagingAlloc, &info);
    memcpy(info.pMappedData, rgba, (size_t)size);

    // --- Image ---
    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_SRGB;
    ici.extent = { w, h, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo iai{};
    iai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VkImage rawImage;
    vmaCreateImage(ctx.allocator(), &ici, &iai, &rawImage, &m_alloc, nullptr);
    m_image = vk::Image(rawImage);

    // --- Copy + transitiony ---
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
        toDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toDst));

        vk::BufferImageCopy region(0, 0, 0,
            { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
            { 0, 0, 0 }, { w, h, 1 });
        cmd.copyBufferToImage(staging, m_image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

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
        toRead.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toRead));
        });

    vmaDestroyBuffer(ctx.allocator(), staging, stagingAlloc);

    // --- ImageView ---
    vk::ImageViewCreateInfo vi({}, m_image, vk::ImageViewType::e2D,
        vk::Format::eR8G8B8A8Srgb, {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    m_view = ctx.device().createImageView(vi);

    // --- Sampler (bez anizotropii – bezpieczne na każdym GPU) ---
    vk::SamplerCreateInfo si({}, vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways);
    m_sampler = ctx.device().createSampler(si);
}

void Texture::destroy(VulkanContext& ctx) {
    if (m_sampler) ctx.device().destroySampler(m_sampler);
    if (m_view)    ctx.device().destroyImageView(m_view);
    if (m_image)   vmaDestroyImage(ctx.allocator(), m_image, m_alloc);
}

// ============================================================
//  GENERATORY PROCEDURALNE
// ============================================================

// --- Trawa ---
std::vector<uint8_t> Texture::genGrass(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            float n = fbm(x * 0.06f, y * 0.06f);
            float n2 = fbm(x * 0.4f + 100.f, y * 0.4f + 50.f, 2);
            glm::vec3 c1(0.18f, 0.40f, 0.14f);
            glm::vec3 c2(0.42f, 0.68f, 0.22f);
            glm::vec3 c3(0.10f, 0.28f, 0.10f);
            glm::vec3 c = glm::mix(c1, c2, n);
            c = glm::mix(c, c3, n2 * 0.35f);
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(glm::clamp(c.r, 0.0f, 1.0f) * 255);
            px[i + 1] = (uint8_t)(glm::clamp(c.g, 0.0f, 1.0f) * 255);
            px[i + 2] = (uint8_t)(glm::clamp(c.b, 0.0f, 1.0f) * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Cegły ---
std::vector<uint8_t> Texture::genBricks(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    int rows = 8, cols = 4;
    int bh = size / rows, bw = size / cols;
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            int row = y / bh;
            int off = (row % 2) ? bw / 2 : 0;
            int bx = (x + off) % bw;
            int by = y % bh;
            bool mortar = (bx < 3) || (by < 3);
            float n = fbm(x * 0.3f, y * 0.3f, 3);
            glm::vec3 c;
            if (mortar) c = glm::vec3(0.72f, 0.70f, 0.65f) * (0.9f + n * 0.15f);
            else {
                float r = 0.55f + n * 0.15f;
                c = glm::vec3(r, r * 0.42f, r * 0.32f);
            }
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(glm::clamp(c.r, 0.f, 1.f) * 255);
            px[i + 1] = (uint8_t)(glm::clamp(c.g, 0.f, 1.f) * 255);
            px[i + 2] = (uint8_t)(glm::clamp(c.b, 0.f, 1.f) * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Kamień ---
std::vector<uint8_t> Texture::genStone(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            float n = fbm(x * 0.08f, y * 0.08f, 5);
            float crack = fbm(x * 0.03f + 50.f, y * 0.03f + 100.f, 3);
            float v = n * 0.8f + 0.2f;
            if (crack < 0.3f) v *= 0.6f;
            glm::vec3 c(v * 0.65f, v * 0.65f, v * 0.70f);
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(glm::clamp(c.r, 0.f, 1.f) * 255);
            px[i + 1] = (uint8_t)(glm::clamp(c.g, 0.f, 1.f) * 255);
            px[i + 2] = (uint8_t)(glm::clamp(c.b, 0.f, 1.f) * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Drewno ---
std::vector<uint8_t> Texture::genWood(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            float rings = std::sin(x * 0.5f + fbm(x * 0.05f, y * 0.05f) * 8.0f);
            float v = 0.55f + rings * 0.15f + fbm(x * 0.2f, y * 0.2f, 3) * 0.15f;
            glm::vec3 c(v * 0.62f, v * 0.40f, v * 0.22f);
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(glm::clamp(c.r, 0.f, 1.f) * 255);
            px[i + 1] = (uint8_t)(glm::clamp(c.g, 0.f, 1.f) * 255);
            px[i + 2] = (uint8_t)(glm::clamp(c.b, 0.f, 1.f) * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Liście ---
std::vector<uint8_t> Texture::genLeaves(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            float n = fbm(x * 0.15f, y * 0.15f, 4);
            float n2 = fbm(x * 0.6f + 33.f, y * 0.6f + 77.f, 2);
            glm::vec3 c1(0.10f, 0.32f, 0.12f);
            glm::vec3 c2(0.36f, 0.62f, 0.22f);
            glm::vec3 c = glm::mix(c1, c2, n);
            if (n2 > 0.75f) c *= 0.6f;
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(glm::clamp(c.r, 0.f, 1.f) * 255);
            px[i + 1] = (uint8_t)(glm::clamp(c.g, 0.f, 1.f) * 255);
            px[i + 2] = (uint8_t)(glm::clamp(c.b, 0.f, 1.f) * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Metal ---
std::vector<uint8_t> Texture::genMetal(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            float n = fbm(x * 0.5f, y * 0.05f, 3);
            float v = 0.55f + n * 0.25f;
            glm::vec3 c(v * 0.72f, v * 0.74f, v * 0.78f);
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(glm::clamp(c.r, 0.f, 1.f) * 255);
            px[i + 1] = (uint8_t)(glm::clamp(c.g, 0.f, 1.f) * 255);
            px[i + 2] = (uint8_t)(glm::clamp(c.b, 0.f, 1.f) * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Checker (szachownica) ---
std::vector<uint8_t> Texture::genChecker(uint32_t size, uint32_t squares,
    glm::vec3 a, glm::vec3 b)
{
    std::vector<uint8_t> px(size * size * 4);
    int cell = size / squares;
    if (cell < 1) cell = 1;
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            bool odd = ((x / cell) + (y / cell)) % 2;
            glm::vec3 c = odd ? b : a;
            size_t i = (y * size + x) * 4;
            px[i + 0] = (uint8_t)(c.r * 255);
            px[i + 1] = (uint8_t)(c.g * 255);
            px[i + 2] = (uint8_t)(c.b * 255);
            px[i + 3] = 255;
        }
    return px;
}

// --- Bullet Hole (czarny ślad po kuli) ---
std::vector<uint8_t> Texture::genBulletHole(uint32_t size) {
    std::vector<uint8_t> px(size * size * 4);
    float c = size * 0.5f;

    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            float dx = (x - c) / c;
            float dy = (y - c) / c;
            float d = std::sqrt(dx * dx + dy * dy);

            // Szum dla efektu "spalenizny"
            float noise = fbm(x * 0.15f, y * 0.15f, 3) * 0.15f;

            float v;   // jasność (0 = czarny, 1 = biały)
            if (d < 0.35f) {
                // Środek – czarna dziura
                v = 0.02f + noise * 0.5f;
            }
            else if (d < 0.55f) {
                // Ciemny pierścień spalenizny
                v = 0.05f + noise * 0.8f;
            }
            else if (d < 0.75f) {
                // Szara obwódka (ślad)
                v = 0.10f + (0.75f - d) * 0.4f + noise;
            }
            else {
                // Reszta – jasna szara (żeby ładnie odcinał się na każdej teksturze)
                v = 0.25f + noise;
            }

            v = glm::clamp(v, 0.0f, 1.0f);
            uint8_t val = (uint8_t)(v * 255);

            size_t i = (y * size + x) * 4;
            px[i + 0] = val;
            px[i + 1] = val;
            px[i + 2] = val;
            px[i + 3] = 255;
        }
    return px;
}