#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include "game/Scene.h"
#include "core/Camera.h"
#include "core/GameState.h"
#include "renderer/Cubemap.h"

class Window;
class VulkanContext;

struct PushConstants {
    glm::mat4 model;
    int32_t   texIndex;
    int32_t   _pad[3];
};

class Renderer {
public:
    Renderer(const Window& w, VulkanContext& ctx, Scene& scene);
    ~Renderer();
    void drawFrame(const Camera& cam, Scene& scene, GameState state,
        const std::function<void()>& uiCallback);
    void waitIdle();

private:
    void initImGui();
    void shutdownImGui();

    void createSurface();
    void createSwapchain();
    void createImageViews();
    void createDepthResources();
    void createCameraUBO();
    void createDescriptorPool();
    void createDescriptorSet(const Scene& scene);
    void createSkyboxPipeline();
    void createPipeline();
    void createCommandResources();
    void createSyncObjects();
    void recreateSwapchain();
    vk::ShaderModule loadShader(const std::string& path);

    void drawWeapon(vk::CommandBuffer cmd, const Camera& cam, const Weapon& weapon);
    void drawDecals(vk::CommandBuffer cmd, const Scene& scene);

    const Window& m_window;
    VulkanContext& m_ctx;
    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapchain;
    vk::Format m_format;
    vk::Extent2D m_extent;

    std::vector<vk::Image> m_images;
    std::vector<vk::ImageView> m_views;

    vk::Image m_depthImage;
    vk::ImageView m_depthView;
    VmaAllocation m_depthAlloc = VK_NULL_HANDLE;
    vk::Format m_depthFormat = vk::Format::eD32Sfloat;

    vk::DescriptorSetLayout m_setLayout;
    vk::DescriptorSet       m_set;
    vk::PipelineLayout      m_layout;
    vk::Pipeline            m_pipeline;

    Cubemap                 m_cubemap;
    vk::DescriptorSetLayout m_skyboxSetLayout;
    vk::DescriptorSet       m_skyboxSet;
    vk::PipelineLayout      m_skyboxLayout;
    vk::Pipeline            m_skyboxPipeline;

    vk::DescriptorPool      m_pool;
    vk::DescriptorPool      m_imguiPool;
    VkBuffer m_camUBO = VK_NULL_HANDLE;
    VmaAllocation m_camUBOAlloc = VK_NULL_HANDLE;
    void* m_camUBOMapped = nullptr;

    vk::CommandPool m_cmdPool;
    std::vector<vk::CommandBuffer> m_cmdBuffers;
    std::vector<vk::Semaphore> m_imageAvailable;
    std::vector<vk::Semaphore> m_renderFinished;
    std::vector<vk::Fence> m_inFlight;
    uint32_t m_frame = 0;
    static constexpr uint32_t MAX_FRAMES = 2;

    bool m_imguiInitialized = false;
};