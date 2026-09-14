#include "renderer/Renderer.h"
#include "renderer/VulkanContext.h"
#include "core/Window.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <volk.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <array>

Renderer::Renderer(const Window& w, VulkanContext& ctx, Scene& scene)
    : m_window(w), m_ctx(ctx)
{
    createSurface();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createCameraUBO();
    createDescriptorPool();
    createDescriptorSet(scene);
    createSkyboxPipeline();
    createPipeline();
    createCommandResources();
    createSyncObjects();
    initImGui();
}

Renderer::~Renderer() {
    m_ctx.device().waitIdle();
    shutdownImGui();

    auto dev = m_ctx.device();
    for (auto s : m_imageAvailable) dev.destroySemaphore(s);
    for (auto s : m_renderFinished) dev.destroySemaphore(s);
    for (auto f : m_inFlight) dev.destroyFence(f);
    dev.destroyCommandPool(m_cmdPool);

    dev.destroyPipeline(m_pipeline);
    dev.destroyPipelineLayout(m_layout);

    dev.destroyPipeline(m_skyboxPipeline);
    dev.destroyPipelineLayout(m_skyboxLayout);
    dev.destroyDescriptorSetLayout(m_skyboxSetLayout);
    m_cubemap.destroy(m_ctx);

    if (m_imguiPool) dev.destroyDescriptorPool(m_imguiPool);
    dev.destroyDescriptorPool(m_pool);
    dev.destroyDescriptorSetLayout(m_setLayout);
    vmaDestroyBuffer(m_ctx.allocator(), m_camUBO, m_camUBOAlloc);
    dev.destroyImageView(m_depthView);
    vmaDestroyImage(m_ctx.allocator(), m_depthImage, m_depthAlloc);
    for (auto v : m_views) dev.destroyImageView(v);
    dev.destroySwapchainKHR(m_swapchain);
    m_ctx.instance().destroySurfaceKHR(m_surface);
}

void Renderer::waitIdle() { m_ctx.device().waitIdle(); }

// ============ IMGUI ============
void Renderer::initImGui() {
    std::cout << "[IMGUI] 1/6 - Descriptor pool..." << std::endl;

    vk::DescriptorPoolSize poolSizes[] = {
        { vk::DescriptorType::eSampler,              1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage,         1000 },
        { vk::DescriptorType::eStorageImage,         1000 },
        { vk::DescriptorType::eUniformTexelBuffer,   1000 },
        { vk::DescriptorType::eStorageTexelBuffer,   1000 },
        { vk::DescriptorType::eUniformBuffer,        1000 },
        { vk::DescriptorType::eStorageBuffer,        1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment,      1000 }
    };
    vk::DescriptorPoolCreateInfo pci({}, 1000, 11, poolSizes);
    pci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_imguiPool = m_ctx.device().createDescriptorPool(pci);

    std::cout << "[IMGUI] 2/6 - Kontekst..." << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    std::cout << "[IMGUI] 3/6 - Czcionki..." << std::endl;

    auto fileExists = [](const char* path) -> bool {
        FILE* f = fopen(path, "rb");
        if (f) { fclose(f); return true; }
        return false;
        };

    const char* uiFont = "C:/Windows/Fonts/segoeui.ttf";
    const char* uiBold = "C:/Windows/Fonts/segoeuib.ttf";

    if (fileExists(uiFont) && fileExists(uiBold)) {
        io.Fonts->AddFontFromFileTTF(uiFont, 14.0f);   // [0] tiny
        io.Fonts->AddFontFromFileTTF(uiFont, 18.0f);   // [1] small
        io.Fonts->AddFontFromFileTTF(uiBold, 22.0f);   // [2] body
        io.Fonts->AddFontFromFileTTF(uiBold, 32.0f);   // [3] large
        io.Fonts->AddFontFromFileTTF(uiBold, 72.0f);   // [4] huge
        std::cout << "[IMGUI]    Zaladowano czcionki Segoe UI" << std::endl;
    }
    else {
        io.Fonts->AddFontDefault();
        std::cout << "[IMGUI]    Brak czcionek systemowych, uzywam domyslnej" << std::endl;
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;

    std::cout << "[IMGUI] 4/6 - Backend GLFW..." << std::endl;

    ImGui_ImplGlfw_InitForVulkan(m_window.handle(), true);

    std::cout << "[IMGUI] 5/6 - LoadFunctions (volk loader)..." << std::endl;

    // ⚠️ ImGui 1.91.5 – LoadFunctions przyjmuje 2 argumenty (loader + user_data)
    VkInstance rawInstance = (VkInstance)m_ctx.instance();
    ImGui_ImplVulkan_LoadFunctions(
        [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
            VkInstance inst = *(VkInstance*)user_data;
            return vkGetInstanceProcAddr(inst, function_name);
        },
        &rawInstance);

    std::cout << "[IMGUI] 6/6 - Vulkan Init..." << std::endl;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_ctx.instance();
    initInfo.PhysicalDevice = m_ctx.physicalDevice();
    initInfo.Device = m_ctx.device();
    initInfo.QueueFamily = m_ctx.graphicsFamily();
    initInfo.Queue = m_ctx.graphicsQueue();
    initInfo.DescriptorPool = m_imguiPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;

    VkFormat colorFmt = (VkFormat)m_format;
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFmt;
    initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    ImGui_ImplVulkan_Init(&initInfo);

    m_imguiInitialized = true;
    std::cout << "[RENDERER] ImGui zainicjalizowany" << std::endl;
}

void Renderer::shutdownImGui() {
    if (m_imguiInitialized) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }
}

// ============ SETUP ============
void Renderer::createSurface() {
    VkSurfaceKHR s;
    if (glfwCreateWindowSurface(m_ctx.instance(), m_window.handle(), nullptr, &s) != VK_SUCCESS)
        throw std::runtime_error("Surface creation failed");
    m_surface = s;
}

void Renderer::createSwapchain() {
    auto caps = m_ctx.physicalDevice().getSurfaceCapabilitiesKHR(m_surface);
    m_extent = caps.currentExtent;
    if (m_extent.width == UINT32_MAX) {
        m_extent = vk::Extent2D{ (uint32_t)m_window.width(), (uint32_t)m_window.height() };
        m_extent.width = std::clamp(m_extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_extent.height = std::clamp(m_extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    auto formats = m_ctx.physicalDevice().getSurfaceFormatsKHR(m_surface);
    m_format = formats[0].format;
    for (auto& f : formats)
        if (f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            m_format = f.format;

    uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && count > caps.maxImageCount) count = caps.maxImageCount;

    vk::SwapchainCreateInfoKHR ci({}, m_surface, count, m_format,
        vk::ColorSpaceKHR::eSrgbNonlinear, m_extent, 1,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive, 0, nullptr,
        caps.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::PresentModeKHR::eFifo, VK_TRUE, nullptr);

    m_swapchain = m_ctx.device().createSwapchainKHR(ci);
    m_images = m_ctx.device().getSwapchainImagesKHR(m_swapchain);
}

void Renderer::createImageViews() {
    m_views.clear();
    for (auto img : m_images) {
        vk::ImageViewCreateInfo ci({}, img, vk::ImageViewType::e2D, m_format, {},
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        m_views.push_back(m_ctx.device().createImageView(ci));
    }
}

void Renderer::createDepthResources() {
    VkImageCreateInfo ci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = (VkFormat)m_depthFormat;
    ci.extent = { m_extent.width, m_extent.height, 1 };
    ci.mipLevels = 1; ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VkImage rawImage = VK_NULL_HANDLE;
    vmaCreateImage(m_ctx.allocator(), &ci, &ai, &rawImage, &m_depthAlloc, nullptr);
    m_depthImage = vk::Image(rawImage);

    vk::ImageViewCreateInfo vi({}, m_depthImage, vk::ImageViewType::e2D, m_depthFormat, {},
        { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
    m_depthView = m_ctx.device().createImageView(vi);
}

void Renderer::createCameraUBO() {
    VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = sizeof(glm::mat4) * 2 + sizeof(glm::vec4);
    bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info;
    vmaCreateBuffer(m_ctx.allocator(), &bi, &ai, &m_camUBO, &m_camUBOAlloc, &info);
    m_camUBOMapped = info.pMappedData;
}

void Renderer::createDescriptorPool() {
    vk::DescriptorPoolSize sizes[] = {
        { vk::DescriptorType::eUniformBuffer,        2 },
        { vk::DescriptorType::eCombinedImageSampler, Scene::MAX_TEXTURES + 1 }
    };
    m_pool = m_ctx.device().createDescriptorPool({ {}, 2, 2, sizes });
}

void Renderer::createDescriptorSet(const Scene& scene) {
    vk::DescriptorSetLayoutBinding meshBindings[2] = {
        { 0, vk::DescriptorType::eUniformBuffer, 1,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eCombinedImageSampler, Scene::MAX_TEXTURES,
          vk::ShaderStageFlagBits::eFragment }
    };
    m_setLayout = m_ctx.device().createDescriptorSetLayout({ {}, 2, meshBindings });

    vk::DescriptorSetLayoutBinding skyboxBindings[2] = {
        { 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },
        { 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment }
    };
    m_skyboxSetLayout = m_ctx.device().createDescriptorSetLayout({ {}, 2, skyboxBindings });

    vk::DescriptorSetLayout layouts[2] = { m_setLayout, m_skyboxSetLayout };
    auto sets = m_ctx.device().allocateDescriptorSets({ m_pool, 2, layouts });
    m_set = sets[0];
    m_skyboxSet = sets[1];

    vk::DescriptorBufferInfo bi(vk::Buffer(m_camUBO), 0, VK_WHOLE_SIZE);
    vk::WriteDescriptorSet uboMesh(m_set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bi);
    vk::WriteDescriptorSet uboSky(m_skyboxSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bi);

    auto& texs = scene.textures();
    if (texs.empty()) throw std::runtime_error("Scene has no textures");

    std::vector<vk::DescriptorImageInfo> imgInfos;
    imgInfos.reserve(Scene::MAX_TEXTURES);
    for (uint32_t i = 0; i < Scene::MAX_TEXTURES; ++i) {
        size_t idx = std::min<size_t>(i, texs.size() - 1);
        imgInfos.push_back({ texs[idx].sampler(), texs[idx].view(),
            vk::ImageLayout::eShaderReadOnlyOptimal });
    }
    vk::WriteDescriptorSet wTex(m_set, 1, 0, (uint32_t)imgInfos.size(),
        vk::DescriptorType::eCombinedImageSampler, imgInfos.data(), nullptr, nullptr);

    m_cubemap.createProcedural(m_ctx, 256);
    vk::DescriptorImageInfo cubeInfo{ m_cubemap.sampler(), m_cubemap.view(),
        vk::ImageLayout::eShaderReadOnlyOptimal };
    vk::WriteDescriptorSet wCube(m_skyboxSet, 1, 0, 1,
        vk::DescriptorType::eCombinedImageSampler, &cubeInfo, nullptr, nullptr);

    vk::WriteDescriptorSet writes[] = { uboMesh, uboSky, wTex, wCube };
    m_ctx.device().updateDescriptorSets(4, writes, 0, nullptr);
}

vk::ShaderModule Renderer::loadShader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open shader: " + path);
    size_t size = (size_t)file.tellg();
    std::vector<char> data(size);
    file.seekg(0); file.read(data.data(), size);
    return m_ctx.device().createShaderModule({ {}, size, (const uint32_t*)data.data() });
}

void Renderer::createPipeline() {
    auto vert = loadShader("shaders/mesh.vert.spv");
    auto frag = loadShader("shaders/mesh.frag.spv");

    vk::PipelineShaderStageCreateInfo stages[] = {
        {{}, vk::ShaderStageFlagBits::eVertex,   vert, "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, frag, "main"}
    };

    auto bind = Vertex::binding();
    auto attrs = Vertex::attributes();
    vk::PipelineVertexInputStateCreateInfo vin({}, 1, &bind, (uint32_t)attrs.size(), attrs.data());

    vk::PipelineInputAssemblyStateCreateInfo ia({}, vk::PrimitiveTopology::eTriangleList);
    vk::PipelineViewportStateCreateInfo vp({}, 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rs({}, VK_FALSE, VK_FALSE,
        vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise, VK_FALSE, 0, 0, 0, 1.0f);
    vk::PipelineMultisampleStateCreateInfo ms({}, vk::SampleCountFlagBits::e1);
    vk::PipelineDepthStencilStateCreateInfo ds({}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess);

    vk::PipelineColorBlendAttachmentState ba(VK_TRUE);
    ba.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo cb({}, VK_FALSE, {}, 1, &ba);

    vk::DynamicState dyn[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dsc({}, 2, dyn);

    vk::PushConstantRange pcr(
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, sizeof(PushConstants));
    m_layout = m_ctx.device().createPipelineLayout({ {}, 1, &m_setLayout, 1, &pcr });

    vk::PipelineRenderingCreateInfo ri({}, 1, &m_format);
    ri.depthAttachmentFormat = m_depthFormat;

    vk::GraphicsPipelineCreateInfo pi({}, 2, stages);
    pi.pVertexInputState = &vin;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;
    pi.pDynamicState = &dsc;
    pi.layout = m_layout;
    pi.pNext = &ri;

    auto result = m_ctx.device().createGraphicsPipeline(nullptr, pi);
    if (result.result != vk::Result::eSuccess) {
        std::cerr << "[RENDERER] Blad tworzenia glownego pipeline: "
            << vk::to_string(result.result) << std::endl;
        throw std::runtime_error("Failed to create main graphics pipeline");
    }
    m_pipeline = result.value;

    m_ctx.device().destroyShaderModule(vert);
    m_ctx.device().destroyShaderModule(frag);
}

void Renderer::createSkyboxPipeline() {
    auto vert = loadShader("shaders/skybox.vert.spv");
    auto frag = loadShader("shaders/skybox.frag.spv");

    vk::PipelineShaderStageCreateInfo stages[] = {
        {{}, vk::ShaderStageFlagBits::eVertex,   vert, "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, frag, "main"}
    };

    vk::PipelineVertexInputStateCreateInfo vin{};
    vk::PipelineInputAssemblyStateCreateInfo ia({}, vk::PrimitiveTopology::eTriangleList);
    vk::PipelineViewportStateCreateInfo vp({}, 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rs({}, VK_FALSE, VK_FALSE,
        vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
        vk::FrontFace::eCounterClockwise, VK_FALSE, 0, 0, 0, 1.0f);
    vk::PipelineMultisampleStateCreateInfo ms({}, vk::SampleCountFlagBits::e1);
    vk::PipelineDepthStencilStateCreateInfo ds({}, VK_TRUE, VK_FALSE, vk::CompareOp::eLessOrEqual);

    vk::PipelineColorBlendAttachmentState ba(VK_FALSE);
    ba.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo cb({}, VK_FALSE, {}, 1, &ba);

    vk::DynamicState dyn[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dsc({}, 2, dyn);

    m_skyboxLayout = m_ctx.device().createPipelineLayout({ {}, 1, &m_skyboxSetLayout });

    vk::PipelineRenderingCreateInfo ri({}, 1, &m_format);
    ri.depthAttachmentFormat = m_depthFormat;

    vk::GraphicsPipelineCreateInfo pi({}, 2, stages);
    pi.pVertexInputState = &vin;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;
    pi.pDynamicState = &dsc;
    pi.layout = m_skyboxLayout;
    pi.pNext = &ri;

    auto result = m_ctx.device().createGraphicsPipeline(nullptr, pi);
    if (result.result != vk::Result::eSuccess) {
        std::cerr << "[RENDERER] Blad tworzenia skybox pipeline: "
            << vk::to_string(result.result) << std::endl;
        throw std::runtime_error("Failed to create skybox pipeline");
    }
    m_skyboxPipeline = result.value;

    m_ctx.device().destroyShaderModule(vert);
    m_ctx.device().destroyShaderModule(frag);
}

void Renderer::createCommandResources() {
    m_cmdPool = m_ctx.device().createCommandPool(
        { vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_ctx.graphicsFamily() });
    m_cmdBuffers = m_ctx.device().allocateCommandBuffers(
        { m_cmdPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES });
}

void Renderer::createSyncObjects() {
    m_imageAvailable.resize(MAX_FRAMES);
    m_renderFinished.resize(m_images.size());
    m_inFlight.resize(MAX_FRAMES);
    for (uint32_t i = 0; i < MAX_FRAMES; ++i) {
        m_imageAvailable[i] = m_ctx.device().createSemaphore({});
        m_inFlight[i] = m_ctx.device().createFence({ vk::FenceCreateFlagBits::eSignaled });
    }
    for (size_t i = 0; i < m_images.size(); ++i)
        m_renderFinished[i] = m_ctx.device().createSemaphore({});
}

void Renderer::recreateSwapchain() {
    m_ctx.device().waitIdle();
    for (auto v : m_views) m_ctx.device().destroyImageView(v);
    m_views.clear();
    for (auto s : m_renderFinished) m_ctx.device().destroySemaphore(s);
    m_renderFinished.clear();
    m_ctx.device().destroyImageView(m_depthView);
    vmaDestroyImage(m_ctx.allocator(), m_depthImage, m_depthAlloc);
    m_ctx.device().destroySwapchainKHR(m_swapchain);

    createSwapchain();
    createImageViews();
    createDepthResources();
    createSyncObjects();
    m_window.resetResizedFlag();
}

// ============ FRAME ============
void Renderer::drawFrame(const Camera& cam, Scene& scene, GameState state,
    const std::function<void()>& uiCallback)
{
    auto dev = m_ctx.device();
    dev.waitForFences(m_inFlight[m_frame], VK_TRUE, UINT64_MAX);

    auto acq = dev.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_imageAvailable[m_frame], nullptr);
    if (acq.result == vk::Result::eErrorOutOfDateKHR) { recreateSwapchain(); return; }
    uint32_t imageIndex = acq.value;
    dev.resetFences(m_inFlight[m_frame]);

    struct CameraData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 camPos;
    } data;
    data.view = cam.view();
    data.proj = cam.projection((float)m_extent.width / (float)m_extent.height);
    data.camPos = glm::vec4(cam.position, 1.0f);
    memcpy(m_camUBOMapped, &data, sizeof(data));
    vmaFlushAllocation(m_ctx.allocator(), m_camUBOAlloc, 0, sizeof(data));

    auto& cmd = m_cmdBuffers[m_frame];
    cmd.reset();
    cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    vk::ImageMemoryBarrier2 toColor;
    toColor.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    toColor.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toColor.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toColor.oldLayout = vk::ImageLayout::eUndefined;
    toColor.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = m_images[imageIndex];
    toColor.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::ImageMemoryBarrier2 toDepth;
    toDepth.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    toDepth.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
    toDepth.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    toDepth.oldLayout = vk::ImageLayout::eUndefined;
    toDepth.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    toDepth.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDepth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDepth.image = m_depthImage;
    toDepth.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 };

    vk::ImageMemoryBarrier2 startBarriers[] = { toColor, toDepth };
    cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(startBarriers));

    vk::ClearValue clearColor;
    clearColor.color = vk::ClearColorValue(std::array<float, 4>{0.05f, 0.06f, 0.10f, 1.0f});

    vk::RenderingAttachmentInfo colorAtt(
        m_views[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
        vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, clearColor);

    vk::ClearValue clearDepth;
    clearDepth.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo depthAtt(
        m_depthView, vk::ImageLayout::eDepthAttachmentOptimal,
        vk::ResolveModeFlagBits::eNone, {}, vk::ImageLayout::eUndefined,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, clearDepth);

    vk::RenderingInfo ri;
    ri.renderArea = vk::Rect2D{ {0,0}, m_extent };
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &colorAtt;
    ri.pDepthAttachment = &depthAtt;
    cmd.beginRendering(ri);

    cmd.setViewport(0, vk::Viewport(0, 0, (float)m_extent.width, (float)m_extent.height, 0, 1));
    cmd.setScissor(0, vk::Rect2D({ 0,0 }, m_extent));

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_layout, 0, 1, &m_set, 0, nullptr);

    auto& objs = scene.objects();
    const auto& meshes = scene.meshes();
    for (auto& obj : objs) {
        obj.updateModel();
        PushConstants pc{};
        pc.model = obj.model;
        pc.texIndex = obj.textureIndex;
        cmd.pushConstants(m_layout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            0, sizeof(PushConstants), &pc);
        meshes[obj.meshIndex].bind(cmd);
        meshes[obj.meshIndex].draw(cmd);
    }

    if (state == GameState::Playing || state == GameState::Paused) {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_skyboxPipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_skyboxLayout, 0, 1,
            &m_skyboxSet, 0, nullptr);
        cmd.draw(36, 1, 0, 0);
    }

    if (m_imguiInitialized) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (uiCallback) uiCallback();

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }

    cmd.endRendering();

    vk::ImageMemoryBarrier2 toPresent;
    toPresent.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toPresent.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toPresent.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    toPresent.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toPresent.newLayout = vk::ImageLayout::ePresentSrcKHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = m_images[imageIndex];
    toPresent.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toPresent));

    cmd.end();

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit(1, &m_imageAvailable[m_frame], &waitStage, 1, &cmd, 1, &m_renderFinished[imageIndex]);
    m_ctx.graphicsQueue().submit(submit, m_inFlight[m_frame]);

    vk::PresentInfoKHR present(1, &m_renderFinished[imageIndex], 1, &m_swapchain, &imageIndex);
    auto res = m_ctx.presentQueue().presentKHR(present);
    if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR || m_window.wasResized())
        recreateSwapchain();

    m_frame = (m_frame + 1) % MAX_FRAMES;
}