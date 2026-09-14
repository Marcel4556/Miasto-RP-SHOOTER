#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "renderer/VulkanContext.h"
#include "core/Window.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <set>
#include <cstring>
#include <stdexcept>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    std::cerr << "[Vulkan] " << data->pMessage << std::endl;
    return VK_FALSE;
}

VulkanContext::VulkanContext(const Window& window) : m_window(window) {
    if (volkInitialize() != VK_SUCCESS)
        throw std::runtime_error("volk init failed");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    std::cout << "[CTX] volk i dispatcher gotowe" << std::endl;

    createInstance();
    std::cout << "[CTX] Instancja utworzona" << std::endl;

    volkLoadInstance(m_instance);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);
    std::cout << "[CTX] Funkcje instancji zaladowane" << std::endl;

    setupDebugMessenger();
    std::cout << "[CTX] Debug messenger OK" << std::endl;

    pickPhysicalDevice();
    std::cout << "[CTX] Fizyczne urzadzenie wybrane" << std::endl;

    createLogicalDevice();
    std::cout << "[CTX] Urzadzenie logiczne utworzone" << std::endl;

    createAllocator();
    std::cout << "[CTX] VMA zainicjalizowany" << std::endl;
}

VulkanContext::~VulkanContext() {
    if (m_allocator) vmaDestroyAllocator(m_allocator);
    if (m_device) m_device.destroy();
    if (m_debugMessenger) m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger);
    if (m_instance) m_instance.destroy();
}

void VulkanContext::createInstance() {
    vk::ApplicationInfo appInfo("VulkanEngine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

    uint32_t glfwCount = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<const char*> extensions(glfwExt, glfwExt + glfwCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());
    bool hasValidation = false;
    for (auto& l : available)
        if (strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) hasValidation = true;

    if (!hasValidation)
        std::cout << "[CTX] UWAGA: warstwa walidacyjna niedostepna - pomijam\n";

    vk::InstanceCreateInfo ci({}, &appInfo);
    ci.setPEnabledExtensionNames(extensions);
    if (hasValidation) ci.setPEnabledLayerNames(layers);

    m_instance = vk::createInstance(ci);
}

void VulkanContext::setupDebugMessenger() {
    vk::DebugUtilsMessengerCreateInfoEXT ci({},
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        debugCallback);
    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(ci);
}

void VulkanContext::pickPhysicalDevice() {
    auto devices = m_instance.enumeratePhysicalDevices();
    if (devices.empty()) throw std::runtime_error("No Vulkan GPU");

    for (auto& d : devices) {
        auto props = d.getProperties();
        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            m_physicalDevice = d;
            break;
        }
    }
    if (!m_physicalDevice) m_physicalDevice = devices[0];

    std::cout << "[CTX] GPU: " << m_physicalDevice.getProperties().deviceName << std::endl;
}

void VulkanContext::createLogicalDevice() {
    auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(m_instance, m_window.handle(), nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Surface creation failed");

    for (uint32_t i = 0; i < (uint32_t)queueFamilies.size(); ++i) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, surface, &presentSupport);
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) m_graphicsFamily = i;
        if (presentSupport) m_presentFamily = i;
    }
    vkDestroySurfaceKHR(m_instance, surface, nullptr);

    std::set<uint32_t> unique = { m_graphicsFamily, m_presentFamily };
    std::vector<vk::DeviceQueueCreateInfo> queueCIs;
    float priority = 1.0f;
    for (uint32_t f : unique)
        queueCIs.push_back({ {}, f, 1, &priority });

    std::vector<const char*> deviceExts = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };

    // Sprawdz czy urzadzenie wspiera samplerAnisotropy i dynamicRendering
    auto supportedFeatures = m_physicalDevice.getFeatures();

    vk::PhysicalDeviceDynamicRenderingFeatures dynRender{ true };
    vk::PhysicalDeviceSynchronization2Features sync2{ true };
    sync2.pNext = &dynRender;

    vk::PhysicalDeviceFeatures features{};
    features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    // KLUCZOWE: wlacz samplerAnisotropy jesli GPU go wspiera
    features.samplerAnisotropy = supportedFeatures.samplerAnisotropy;
    std::cout << "[CTX] samplerAnisotropy wspierany: "
        << (supportedFeatures.samplerAnisotropy ? "TAK" : "NIE") << std::endl;

    vk::DeviceCreateInfo ci({}, queueCIs);
    ci.setPEnabledExtensionNames(deviceExts);
    ci.pNext = &sync2;
    ci.setPEnabledFeatures(&features);

    m_device = m_physicalDevice.createDevice(ci);
    volkLoadDevice(m_device);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

    m_graphicsQueue = m_device.getQueue(m_graphicsFamily, 0);
    m_presentQueue = m_device.getQueue(m_presentFamily, 0);
}

void VulkanContext::createAllocator() {
    VmaVulkanFunctions funcs{};
    funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo ci{};
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.instance = m_instance;
    ci.physicalDevice = m_physicalDevice;
    ci.device = m_device;
    ci.pVulkanFunctions = &funcs;

    if (vmaCreateAllocator(&ci, &m_allocator) != VK_SUCCESS)
        throw std::runtime_error("VMA creation failed");
}

void VulkanContext::submitImmediate(const std::function<void(vk::CommandBuffer)>& fn) {
    vk::CommandPoolCreateInfo pci({ vk::CommandPoolCreateFlagBits::eTransient, m_graphicsFamily });
    auto pool = m_device.createCommandPool(pci);
    auto cmd = m_device.allocateCommandBuffers({ pool, vk::CommandBufferLevel::ePrimary, 1 })[0];

    cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    fn(cmd);
    cmd.end();

    vk::SubmitInfo si(0, nullptr, nullptr, 1, &cmd);
    m_graphicsQueue.submit(si);
    m_graphicsQueue.waitIdle();

    m_device.destroyCommandPool(pool);
}