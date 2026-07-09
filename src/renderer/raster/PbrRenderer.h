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

// 셰이더 pbr.slang의 PushConstants와 레이아웃 일치.
// (RTX급 GPU의 maxPushConstantsSize=256 이내. 이식성보다 단순함 우선)
struct PushConstants {
    glm::mat4 viewProj;             // 0
    uint64_t vertexBufferAddress;   // 64
    uint64_t materialBufferAddress; // 72
    glm::vec4 cameraPos;            // 80
    glm::vec4 sunDirection;         // 96  (빛 진행 방향)
    glm::vec4 sunColor;             // 112 (rgb, w=intensity)
    glm::vec4 ambient;              // 128 (rgb, w=intensity)
    uint32_t materialIndex;         // 144
    uint32_t _pad[3];               // 148
};

class PbrRenderer : public IRenderer {
public:
    // mesh/camera는 ZeroApp이 소유하고 주입 (렌더러 간 공유)
    PbrRenderer(VulkanContext* context, Swapchain* swapchain, Mesh* mesh, Camera* camera);
    ~PbrRenderer() override;

    void init() override;
    void onResize(uint32_t width, uint32_t height) override;
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) override;
    void onUI() override;

    Camera* getCamera() const override { return m_camera; }

private:
    VulkanContext* m_context;
    Swapchain* m_swapchain;

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;
    std::unique_ptr<DescriptorManager> m_descriptorManager;

    Camera* m_camera;   // non-owning
    Mesh* m_mesh;       // non-owning

    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline m_pipeline{ VK_NULL_HANDLE };

    // 라이팅 파라미터 (ImGui로 조절)
    glm::vec3 m_sunDirection{ -0.4f, -1.0f, -0.3f };
    glm::vec3 m_sunColor{ 1.0f, 0.98f, 0.92f };
    float m_sunIntensity{ 4.0f };
    glm::vec3 m_ambientColor{ 0.5f, 0.6f, 0.75f };
    float m_ambientIntensity{ 0.25f };
};


#endif
