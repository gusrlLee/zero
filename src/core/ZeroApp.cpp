#include "core/ZeroApp.h"
#include "core/Window.h"
#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "renderer/IRenderer.h"
#include "renderer/UIOverlay.h"
#include "util/Log.h"

#include "renderer/raster/PbrRenderer.h"
#include "renderer/pathtracing/PathTracer.h"
#include "scene/Camera.h"
#include "scene/Mesh.h"

#include <imgui.h>

#include <SDL3/SDL.h>
#include <iostream>

ZeroApp::ZeroApp() : m_isRunning(false), m_framebufferResized(false) {
    init();
}

ZeroApp::~ZeroApp() {
    cleanup();
}

void ZeroApp::init() {
    // 1. Core systems initialization
    m_window = std::make_unique<Window>(1920, 1080, "Zero Engine");
    m_vulkanContext = std::make_unique<VulkanContext>(m_window.get());
    m_swapchain = std::make_unique<Swapchain>(m_vulkanContext.get(), m_window.get());

    // 2. Command buffer & synchronization objects
    createFrameData();

    // 3. 공유 씬 리소스: 카메라 + 메시(한 번만 로드)
    VkExtent2D extent = m_swapchain->getExtent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    m_camera = std::make_unique<Camera>(45.0f, aspect, 0.1f, 10000.0f);
    m_mesh = std::make_unique<Mesh>(m_vulkanContext.get(), "../assets/bistro/bistro.gltf");

    // 4. 렌더러 (기본: PBR 래스터, F2로 패스트레이서 전환)
    setRenderer(m_useRayTracer);

    // 5. UI 오버레이 (ImGui)
    m_ui = std::make_unique<UIOverlay>(m_vulkanContext.get(), m_window.get(), m_swapchain.get());

    m_isRunning = true;
    std::cout << "[Zero Engine] Initialization successfully completed." << std::endl;
}

void ZeroApp::createFrameData() {
    VkDevice device = m_vulkanContext->getDevice();
    m_frames.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandPoolCreateInfo poolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_vulkanContext->getGraphicsQueueFamily()
    };

    VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceCI{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        CHK(vkCreateCommandPool(device, &poolCI, nullptr, &m_frames[i].commandPool));

        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_frames[i].commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        CHK(vkAllocateCommandBuffers(device, &allocInfo, &m_frames[i].commandBuffer));

        CHK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_frames[i].imageAvailableSemaphore));
        CHK(vkCreateFence(device, &fenceCI, nullptr, &m_frames[i].inFlightFence));
    }

    uint32_t imageCount = m_swapchain->getImageCount();
    m_renderFinishedSemaphores.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        CHK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_renderFinishedSemaphores[i]));
    }
}

void ZeroApp::setRenderer(bool useRayTracer) {
    CHK(vkDeviceWaitIdle(m_vulkanContext->getDevice()));
    m_renderer.reset(); // 기존 렌더러 파괴 (mesh/camera는 ZeroApp이 계속 소유)

    if (useRayTracer) {
        m_renderer = std::make_unique<PathTracer>(
            m_vulkanContext.get(), m_swapchain.get(), m_mesh.get(), m_camera.get());
    }
    else {
        m_renderer = std::make_unique<PbrRenderer>(
            m_vulkanContext.get(), m_swapchain.get(), m_mesh.get(), m_camera.get());
    }
    m_renderer->init();
    m_useRayTracer = useRayTracer;
    std::cout << "[Zero Engine] Renderer: " << (useRayTracer ? "Path Tracer" : "PBR Raster") << std::endl;
}

void ZeroApp::run() {
    mainLoop();
}

void ZeroApp::mainLoop() {
    uint64_t lastTime = SDL_GetPerformanceCounter();

    // 시작은 카메라 시점 조작 모드 (마우스 캡처). F1으로 UI 조작 모드와 토글.
    SDL_SetWindowRelativeMouseMode(m_window->getNativeHandle(), true);

    while (!m_window->shouldClose()) {
        uint64_t currentTime = SDL_GetPerformanceCounter();
        float deltaTime = (float)(currentTime - lastTime) / SDL_GetPerformanceFrequency();
        lastTime = currentTime;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_ui->processEvent(&event); // ImGui가 먼저 이벤트를 확인

            if (event.type == SDL_EVENT_QUIT) {
                m_window->close();
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                m_window->close();
            }
            // F1: 카메라 시점 모드 <-> UI 조작 모드 토글
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1 && !event.key.repeat) {
                m_cursorEnabled = !m_cursorEnabled;
                SDL_SetWindowRelativeMouseMode(m_window->getNativeHandle(), !m_cursorEnabled);
            }
            // F2: PBR 래스터 <-> 패스트레이서 전환
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F2 && !event.key.repeat) {
                setRenderer(!m_useRayTracer);
            }

            // UI 조작 모드가 아니고 ImGui가 입력을 잡고 있지 않을 때만 카메라 조작
            bool cameraInput = !m_cursorEnabled && !m_ui->wantCaptureMouse() && !m_ui->wantCaptureKeyboard();
            if (cameraInput) {
                if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    m_renderer->getCamera()->processMouse(event.motion.xrel, event.motion.yrel);
                }
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    m_renderer->getCamera()->processKeyboard(event.key.scancode, true);
                }
                if (event.type == SDL_EVENT_KEY_UP) {
                    m_renderer->getCamera()->processKeyboard(event.key.scancode, false);
                }
            }
        }

        m_renderer->getCamera()->update(deltaTime);
        buildUI(deltaTime);
        drawFrame();
    }
    CHK(vkDeviceWaitIdle(m_vulkanContext->getDevice()));
}

void ZeroApp::buildUI(float deltaTime) {
    m_ui->beginFrame();

    if (ImGui::Begin("Zero Engine")) {
        float fps = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
        ImGui::Text("%.1f FPS (%.2f ms)", fps, deltaTime * 1000.0f);
        ImGui::Text("Renderer: %s  (F2 to switch)", m_useRayTracer ? "Path Tracer" : "PBR Raster");
        ImGui::Text("Mode: %s", m_cursorEnabled ? "UI (F1: camera)" : "Camera (F1: UI)");
        ImGui::Separator();
    }
    ImGui::End();

    // 각 렌더러가 자신의 패널을 추가
    m_renderer->onUI();
}

void ZeroApp::drawFrame() {
    VkDevice device = m_vulkanContext->getDevice();
    FrameData& frame = m_frames[m_currentFrame];

    // 1. Wait for the previous frame's GPU operations to complete
    CHK(vkWaitForFences(device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));

    // 2. Acquire the next image from the swapchain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, m_swapchain->getHandle(), UINT64_MAX,
        frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // Recreate swapchain if window resized or swapchain became invalid
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        ImGui::EndFrame(); // buildUI에서 시작한 프레임을 렌더 없이 마무리 (NewFrame/Render 짝 유지)
        recreateSwapchain();
        return; // Skip this frame and try again next loop
    }
    else {
        CHK(result);
    }

    // Only reset the fence if we are actually submitting work
    CHK(vkResetFences(device, 1, &frame.inFlightFence));

    // 3. Record command buffer
    CHK(vkResetCommandBuffer(frame.commandBuffer, 0));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    CHK(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo));

    // Delegate actual drawing to the renderer logic.
    // 렌더러는 스왑체인 이미지를 ATTACHMENT_OPTIMAL 상태로 남긴다.
    if (m_renderer) {
        m_renderer->recordCommands(frame.commandBuffer, imageIndex);
    }

    // UI 오버레이를 씬 위에 덧그림 (ATTACHMENT_OPTIMAL 유지)
    m_ui->render(frame.commandBuffer, m_swapchain->getImageView(imageIndex), m_swapchain->getExtent());

    // 스왑체인 이미지를 ATTACHMENT_OPTIMAL -> PRESENT_SRC로 전이
    VkImageMemoryBarrier2 barrierToPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = m_swapchain->getImage(imageIndex),
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo dependencyInfoToPresent{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierToPresent
    };
    vkCmdPipelineBarrier2(frame.commandBuffer, &dependencyInfoToPresent);

    CHK(vkEndCommandBuffer(frame.commandBuffer));

    // 4. Submit command buffer to the graphics queue
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.imageAvailableSemaphore,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_renderFinishedSemaphores[imageIndex]
    };

    CHK(vkQueueSubmit(m_vulkanContext->getGraphicsQueue(), 1, &submitInfo, frame.inFlightFence));

    // 5. Present the rendered image to the screen
    VkSwapchainKHR swapchains[] = { m_swapchain->getHandle() };
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex
    };

    result = vkQueuePresentKHR(m_vulkanContext->getGraphicsQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
    }
    else {
        CHK(result);
    }

    // Advance to the next frame
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void ZeroApp::recreateSwapchain() {
    int width = 0, height = 0;
    SDL_GetWindowSize(m_window->getNativeHandle(), &width, &height);

    // Pause rendering if window is minimized (width or height is 0)
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(m_window->getNativeHandle(), &width, &height);
        m_window->pollEvents();
    }

    CHK(vkDeviceWaitIdle(m_vulkanContext->getDevice()));

    for (auto sem : m_renderFinishedSemaphores) {
        vkDestroySemaphore(m_vulkanContext->getDevice(), sem, nullptr);
    }

    m_swapchain->recreate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    uint32_t imageCount = m_swapchain->getImageCount();
    m_renderFinishedSemaphores.resize(imageCount);
    VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (uint32_t i = 0; i < imageCount; i++) {
        CHK(vkCreateSemaphore(m_vulkanContext->getDevice(), &semaphoreCI, nullptr, &m_renderFinishedSemaphores[i]));
    }

    if (m_camera) {
        m_camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }
    if (m_renderer) {
        m_renderer->onResize(width, height);
    }
}

void ZeroApp::cleanup() {
    std::cout << "[Zero Engine] Shutting down..." << std::endl;

    if (m_vulkanContext) {
        VkDevice device = m_vulkanContext->getDevice();
        CHK(vkDeviceWaitIdle(device));

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroyFence(device, m_frames[i].inFlightFence, nullptr);
            vkDestroyCommandPool(device, m_frames[i].commandPool, nullptr);
        }

        for (auto sem : m_renderFinishedSemaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
}