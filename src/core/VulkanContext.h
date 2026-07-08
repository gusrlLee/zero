#ifndef __ZERO_VULKAN_CONTEXT_HEADER__
#define __ZERO_VULKAN_CONTEXT_HEADER__

#include <vulkan/vulkan.h>
#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <string>
#include <vector>

class Window;

class VulkanContext {
    public:
        VulkanContext(Window* window);
        ~VulkanContext();

        VkInstance getInstance() const { return m_instance; }
        VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
        VkDevice getDevice() const { return m_device; }
        VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
        VkSurfaceKHR getSurface() const { return m_surface; }
        VmaAllocator getAllocator() const { return m_allocator; }
        uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    private:
        void createInstance(Window* window);
        void createSurface(Window* window);
        void selectPhysicalDevice();
        void createLogicalDevice();
        void initVma();

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
        VkDevice m_device{ VK_NULL_HANDLE };
        VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
        VmaAllocator m_allocator{ VK_NULL_HANDLE };

        uint32_t m_graphicsQueueFamily{ 0 };

        VkCommandPool m_transferCommandPool{ VK_NULL_HANDLE };
    };

#endif