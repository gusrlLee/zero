#ifndef __ZERO_APPLICATION_HEADER__
#define __ZERO_APPLICATION_HEADER__

#include <memory>
#include <vector>
#include <volk/volk.h>

class Window;
class VulkanContext;
class Swapchain;
class IRenderer;

struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore; // GPU: "Swapchain image is ready!"
    VkFence inFlightFence;               // CPU: "Wait here until GPU finishes this frame"
};

class ZeroApp {
    public:
        ZeroApp();
        ~ZeroApp();

        ZeroApp(const ZeroApp&) = delete;
        ZeroApp& operator=(const ZeroApp&) = delete;

        void run();

    private:
        void init();
        void mainLoop();
        void cleanup();

        void drawFrame();
        void createFrameData();
        void recreateSwapchain();

        std::unique_ptr<Window> m_window;
        std::unique_ptr<VulkanContext> m_vulkanContext;
        std::unique_ptr<Swapchain> m_swapchain;
        std::unique_ptr<IRenderer> m_renderer;

        bool m_isRunning;
        bool m_framebufferResized;

        // Double buffering synchronization
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t m_currentFrame = 0;
        std::vector<FrameData> m_frames;

        std::vector<VkSemaphore> m_renderFinishedSemaphores;
};

#endif
