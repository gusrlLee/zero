#ifndef __ZERO_APPLICATION_HEADER__
#define __ZERO_APPLICATION_HEADER__

#include <iostream>

class Window;
class VulkanContext;
class VulkanRenderer;
class Scene;

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

    std::unique_ptr<Window> m_window;
    std::unique_ptr<VulkanContext> m_vulkanContext;
    // std::unique_ptr<VulkanRenderer> m_vulkanRenderer;
    // std::unique_ptr<Scene> m_scene;
        
        bool m_isRunning;

};

#endif
