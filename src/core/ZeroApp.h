#ifndef __ZERO_APPLICATION_HEADER__
#define __ZERO_APPLICATION_HEADER__

#include <memory>
#include <vector>
#include <volk/volk.h>

class Window;
class VulkanContext;
class Swapchain;
class IRenderer;
class UIOverlay;
class Mesh;
class Camera;

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
        void setRenderer(bool useRayTracer); // 렌더러 교체 (mesh/camera 공유)

        std::unique_ptr<Window> m_window;
        std::unique_ptr<VulkanContext> m_vulkanContext;
        std::unique_ptr<Swapchain> m_swapchain;
        std::unique_ptr<Camera> m_camera;   // 렌더러 간 공유 (ZeroApp 소유)
        std::unique_ptr<Mesh> m_mesh;       // 한 번만 로드
        std::unique_ptr<IRenderer> m_renderer;
        std::unique_ptr<UIOverlay> m_ui;

        bool m_isRunning;
        bool m_framebufferResized;
        bool m_cursorEnabled = false;  // true면 마우스로 UI 조작, false면 카메라 시점 조작
        bool m_useRayTracer = false;   // false: PBR 래스터, true: 패스트레이서

        void buildUI(float deltaTime);

        // Double buffering synchronization
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t m_currentFrame = 0;
        std::vector<FrameData> m_frames;

        std::vector<VkSemaphore> m_renderFinishedSemaphores;
};

#endif
