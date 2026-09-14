#pragma once
#include <volk.h>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <functional>

class Window;

class VulkanContext {
public:
    VulkanContext(const Window& window);
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    vk::Instance instance() const { return m_instance; }
    vk::PhysicalDevice physicalDevice() const { return m_physicalDevice; }
    vk::Device device() const { return m_device; }
    vk::Queue graphicsQueue() const { return m_graphicsQueue; }
    vk::Queue presentQueue() const { return m_presentQueue; }
    uint32_t graphicsFamily() const { return m_graphicsFamily; }
    uint32_t presentFamily() const { return m_presentFamily; }
    VmaAllocator allocator() const { return m_allocator; }

    void submitImmediate(const std::function<void(vk::CommandBuffer)>& fn);

private:
    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();

    const Window& m_window;
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debugMessenger;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::Queue m_graphicsQueue, m_presentQueue;
    uint32_t m_graphicsFamily = 0, m_presentFamily = 0;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};