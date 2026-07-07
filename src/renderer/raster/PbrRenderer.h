#ifndef __ZERO_PHYSICAL_BAESD_RENDERER_HEADER__
#define __ZERO_PHYSICAL_BAESD_RENDERER_HEADER__

#include "renderer/IRenderer.h"
#include <volk/volk.h>
#include <memory>

class VulkanContext;
class Swapchain;
class ShaderCompiler;

class PbrRenderer : public IRenderer {
public:
    PbrRenderer(VulkanContext* context, Swapchain* swapchain);
    ~PbrRenderer() override;

    void init() override;
    void onResize(uint32_t width, uint32_t height) override;
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) override;
    void onUI() override;

private:
    VulkanContext* m_context;
    Swapchain* m_swapchain;

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;

    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline m_pipeline{ VK_NULL_HANDLE };
};


#endif
