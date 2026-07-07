#include "core/VulkanContext.h"
#include "core/Window.h"

#define VOLK_IMPLEMENTATION
#include <volk/volk.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "util/Log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>

VulkanContext::VulkanContext(Window* window) {
    CHK(volkInitialize());

    createInstance(window);
    volkLoadInstance(m_instance);

    createSurface(window);
    selectPhysicalDevice();
    createLogicalDevice();
    initVma();
}

VulkanContext::~VulkanContext() {
    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::createInstance(Window* window) {
    uint32_t extensionCount{ 0 };
    char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&extensionCount) };

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Zero Engine",
        .apiVersion = VK_API_VERSION_1_3 // Stable 1.3 for VMA compatibility and reference standard
    };

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = instanceExtensions,
    };

    CHK(vkCreateInstance(&createInfo, nullptr, &m_instance));
}

void VulkanContext::createSurface(Window* window) {
    CHK(SDL_Vulkan_CreateSurface(window->getNativeHandle(), m_instance, nullptr, &m_surface));
}

void VulkanContext::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    CHK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
    CHK(deviceCount > 0); // Ensure at least one Vulkan-compatible GPU is found

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CHK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()));

    // Temporarily select the first device (TODO: Add logic to evaluate ray tracing support later)
    m_physicalDevice = devices[0];

    VkPhysicalDeviceProperties2 deviceProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &deviceProperties);
    std::cout << "[Zero Engine] Selected GPU: " << deviceProperties.properties.deviceName << "\n";

    // Find a graphics queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool found = false;
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            // Check if this queue also supports presentation to the SDL window
            if (SDL_Vulkan_GetPresentationSupport(m_instance, m_physicalDevice, i)) {
                m_graphicsQueueFamily = i;
                found = true;
                break;
            }
        }
    }

    CHK(found); // Ensure a suitable graphics/present queue family was found
}

void VulkanContext::createLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCI{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_graphicsQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    // ★ Core Feature Activation Chain for Modern Rendering & Ray Tracing ★

    VkPhysicalDeviceVulkan11Features enabledVk11Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .shaderDrawParameters = VK_TRUE 
    };

    // 1. Buffer Device Address (BDA) & Descriptor Indexing
    VkPhysicalDeviceVulkan12Features enabledVk12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &enabledVk11Features, 
        .descriptorIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE
    };

    // 2. Dynamic Rendering & Sync2
    VkPhysicalDeviceVulkan13Features enabledVk13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features, // Chain link 1
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    // 3. Acceleration Structure
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &enabledVk13Features, // Chain link 2
        .accelerationStructure = VK_TRUE
    };

    // 4. Ray Tracing Pipeline
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &accelStructFeatures, // Chain link 3
        .rayTracingPipeline = VK_TRUE
    };

    // 5. Ray Query (Inline Ray Tracing in Compute/Fragment shaders)
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = &rtPipelineFeatures, // Chain link 4
        .rayQuery = VK_TRUE
    };

    // 6. KHR Cooperative Matrix (For Tensor Core acceleration & Neural Rendering)
    VkPhysicalDeviceCooperativeMatrixFeaturesKHR coopMatrixFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR,
        .pNext = &rayQueryFeatures, // Chain link 5
        .cooperativeMatrix = VK_TRUE
    };

    // Basic features
    VkPhysicalDeviceFeatures enabledVk10Features{
        .samplerAnisotropy = VK_TRUE
    };

    // Add all the required device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // Required by ray tracing pipeline
        VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME        // Standard KHR extension for Tensor Cores
    };

    VkDeviceCreateInfo deviceCI{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        // The head of our massive pNext chain goes here!
        .pNext = &coopMatrixFeatures,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &enabledVk10Features
    };

    CHK(vkCreateDevice(m_physicalDevice, &deviceCI, nullptr, &m_device));

    volkLoadDevice(m_device); // Load device-specific function pointers
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
}

void VulkanContext::initVma() {
    VmaVulkanFunctions vkFunctions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage
    };

    VmaAllocatorCreateInfo allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT, // ★ Required for BDA
        .physicalDevice = m_physicalDevice,
        .device = m_device,
        .pVulkanFunctions = &vkFunctions,
        .instance = m_instance,
        .vulkanApiVersion = VK_API_VERSION_1_3
    };

    CHK(vmaCreateAllocator(&allocatorInfo, &m_allocator));
}