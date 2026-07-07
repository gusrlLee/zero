#ifndef __ZERO_VULKAN_CONTEXT_HEADER__
#define __ZERO_VULKAN_CONTEXT_HEADER__

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <vector>

class Window;

class VulkanContext {
    public:
        VulkanContext(Window* window);
        ~VulkanContext();

        VkInstance getInstance() const { return m_instance; }
        VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
        VkDevice getDevice() const { return m_device; }
        VmaAllocator getAllocator() const { return m_allocator; }
        uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }

    private:
        void createInstance(Window* window);
        void selectPhysicalDevice();
        void createLogicalDevice();
        void initVma();

        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_device;
        VkQueue m_graphicsQueue;
        uint32_t m_graphicsQueueFamily;
        VmaAllocator m_allocator;
    };

#endif