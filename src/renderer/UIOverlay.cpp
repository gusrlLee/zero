#include "renderer/UIOverlay.h"
#include "core/VulkanContext.h"
#include "core/Window.h"
#include "core/Swapchain.h"
#include "util/Log.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

#include <SDL3/SDL.h>

// volk가 로드한 함수 포인터를 ImGui Vulkan 백엔드에 공급 (VK_NO_PROTOTYPES 대응)
static PFN_vkVoidFunction imguiVulkanLoader(const char* functionName, void* userData) {
    VkInstance instance = reinterpret_cast<VkInstance>(userData);
    return vkGetInstanceProcAddr(instance, functionName);
}

UIOverlay::UIOverlay(VulkanContext* context, Window* window, Swapchain* swapchain)
    : m_context(context) {
    VkDevice device = context->getDevice();

    // 1. ImGui가 사용할 디스크립터 풀 (폰트/텍스처용 COMBINED_IMAGE_SAMPLER)
    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 };
    VkDescriptorPoolCreateInfo poolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    CHK(vkCreateDescriptorPool(device, &poolCI, nullptr, &m_pool));

    // 2. ImGui 컨텍스트 + 스타일
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // 3. SDL3 플랫폼 백엔드
    ImGui_ImplSDL3_InitForVulkan(window->getNativeHandle());

    // 4. Vulkan 렌더 백엔드 — volk 함수 포인터 공급 후 dynamic rendering으로 초기화
    ImGui_ImplVulkan_LoadFunctions(imguiVulkanLoader, context->getInstance());

    VkFormat colorFormat = swapchain->getImageFormat();
    VkPipelineRenderingCreateInfo renderingCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat
    };

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = context->getInstance();
    initInfo.PhysicalDevice = context->getPhysicalDevice();
    initInfo.Device = device;
    initInfo.QueueFamily = context->getGraphicsQueueFamily();
    initInfo.Queue = context->getGraphicsQueue();
    initInfo.DescriptorPool = m_pool;
    initInfo.MinImageCount = swapchain->getImageCount();
    initInfo.ImageCount = swapchain->getImageCount();
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = renderingCI;

    ImGui_ImplVulkan_Init(&initInfo);
}

UIOverlay::~UIOverlay() {
    // 렌더링이 끝난 뒤 파괴됨 (ZeroApp이 vkDeviceWaitIdle 이후 정리)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (m_pool) vkDestroyDescriptorPool(m_context->getDevice(), m_pool, nullptr);
}

void UIOverlay::processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
}

void UIOverlay::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void UIOverlay::render(VkCommandBuffer cmd, VkImageView targetView, VkExtent2D extent) {
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();

    VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = targetView,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // 씬 위에 덧그림
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { {0, 0}, extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment
    };

    vkCmdBeginRendering(cmd, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
    vkCmdEndRendering(cmd);
}

bool UIOverlay::wantCaptureMouse() const { return ImGui::GetIO().WantCaptureMouse; }
bool UIOverlay::wantCaptureKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
