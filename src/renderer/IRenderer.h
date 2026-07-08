#ifndef __ZERO_RENDERER_HEADER__
#define __ZERO_RENDERER_HEADER__

#include <volk/volk.h>
#include <cstdint>

class Camera;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Initialize renderer-specific resources (Pipelines, Descriptors, etc.)
    virtual void init() = 0;

    // Called when the swapchain is recreated (Window resize)
    virtual void onResize(uint32_t width, uint32_t height) = 0;

    // Core logic: Record drawing commands into the provided command buffer
    virtual void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) = 0;

    // For ImGui parameter tuning
    virtual void onUI() {}

    virtual Camera* getCamera() const = 0;
};

#endif