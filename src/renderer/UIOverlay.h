#ifndef __ZERO_UI_OVERLAY_HEADER__
#define __ZERO_UI_OVERLAY_HEADER__

#include <volk/volk.h>

union SDL_Event;

class VulkanContext;
class Window;
class Swapchain;

// Dear ImGui overlay: SDL3 platform + Vulkan(dynamic rendering) renderer backend.
// The scene renderer leaves the swapchain image in ATTACHMENT_OPTIMAL; this draws
// the UI on top in a separate loadOp=LOAD rendering pass.
class UIOverlay {
public:
    UIOverlay(VulkanContext* context, Window* window, Swapchain* swapchain);
    ~UIOverlay();

    UIOverlay(const UIOverlay&) = delete;
    UIOverlay& operator=(const UIOverlay&) = delete;

    void processEvent(const SDL_Event* event);
    void beginFrame(); // ImGui::NewFrame — build widgets after this, before render()
    void render(VkCommandBuffer cmd, VkImageView targetView, VkExtent2D extent);

    bool wantCaptureMouse() const;
    bool wantCaptureKeyboard() const;

private:
    VulkanContext* m_context;
    VkDescriptorPool m_pool{ VK_NULL_HANDLE };
};

#endif
