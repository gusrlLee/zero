#include "core/ZeroApp.h"
#include "core/Window.h"
#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "renderer/IRenderer.h"
#include "util/Log.h"

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

    // 3. Renderer initialization (TODO: Plug in actual renderer later)
    // m_renderer = std::make_unique<PbrRenderer>(m_vulkanContext.get(), m_swapchain.get());
    // m_renderer->init();

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
        .flags = VK_FENCE_CREATE_SIGNALED_BIT // Start signaled so the first frame doesn't block forever
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
        CHK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_frames[i].renderFinishedSemaphore));
        CHK(vkCreateFence(device, &fenceCI, nullptr, &m_frames[i].inFlightFence));
    }
}

void ZeroApp::run() {
    mainLoop();
}

void ZeroApp::mainLoop() {
    while (!m_window->shouldClose()) {
        m_window->pollEvents();

        // Window class needs a way to detect resize events.
        // For now, we assume SDL polling updates this flag.
        // if (m_window->wasResized()) { m_framebufferResized = true; }

        drawFrame();
    }

    // Wait until GPU finishes all queued operations before destroying objects
    CHK(vkDeviceWaitIdle(m_vulkanContext->getDevice()));
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

    // Delegate actual drawing to the renderer logic
    if (m_renderer) {
        m_renderer->recordCommands(frame.commandBuffer, imageIndex);
    }

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
        .pSignalSemaphores = &frame.renderFinishedSemaphore
    };

    CHK(vkQueueSubmit(m_vulkanContext->getGraphicsQueue(), 1, &submitInfo, frame.inFlightFence));

    // 5. Present the rendered image to the screen
    VkSwapchainKHR swapchains[] = { m_swapchain->getHandle() };
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.renderFinishedSemaphore,
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

    m_swapchain->recreate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    if (m_renderer) {
        m_renderer->onResize(width, height);
    }
}

void ZeroApp::cleanup() {
    std::cout << "[Zero Engine] Shutting down..." << std::endl;

    if (m_vulkanContext) {
        VkDevice device = m_vulkanContext->getDevice();
        CHK(vkDeviceWaitIdle(device));

        // Manually destroy Vulkan handles inside FrameData
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, m_frames[i].renderFinishedSemaphore, nullptr);
            vkDestroySemaphore(device, m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroyFence(device, m_frames[i].inFlightFence, nullptr);
            vkDestroyCommandPool(device, m_frames[i].commandPool, nullptr);
        }
    }
    // unique_ptrs (Renderer, Swapchain, VulkanContext, Window) will automatically release memory here.
}