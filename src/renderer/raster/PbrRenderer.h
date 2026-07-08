#ifndef __ZERO_PHYSICAL_BAESD_RENDERER_HEADER__
#define __ZERO_PHYSICAL_BAESD_RENDERER_HEADER__

#include "renderer/IRenderer.h"
#include "scene/Camera.h"
#include "scene/Mesh.h"
#include <volk/volk.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>

class Buffer;
class VulkanContext;
class Swapchain;
class ShaderCompiler;
class DescriptorManager;

struct PushConstants {
    glm::mat4 viewProj;          // 64바이트 (카메라 매트릭스를 직접 전송)
    uint64_t vertexBufferAddress;// 8바이트
    uint32_t textureIndex;       // 4바이트
};

class PbrRenderer : public IRenderer {
public:
    PbrRenderer(VulkanContext* context, Swapchain* swapchain);
    ~PbrRenderer() override;

    void init() override;
    void onResize(uint32_t width, uint32_t height) override;
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) override;
    void onUI() override;

    Camera* getCamera() const override { return m_camera.get(); }

private:
    VulkanContext* m_context;
    Swapchain* m_swapchain;

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;
    std::unique_ptr<DescriptorManager> m_descriptorManager;

    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Mesh> m_mesh;

    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline m_pipeline{ VK_NULL_HANDLE };
};


#endif
