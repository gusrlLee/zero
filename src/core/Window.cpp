#include "core/Window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

Window::Window(int width, int height, const std::string& title)
    : m_width(width), m_height(height), m_shouldClose(false) {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error("SDL3 초기화 실패: " + std::string(SDL_GetError()));
    }

    // Vulkan 사용을 위한 SDL_WINDOW_VULKAN 플래그 필수
    m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN);
    
    if (!m_window) {
        throw std::runtime_error("창 생성 실패: " + std::string(SDL_GetError()));
    }
}

Window::~Window() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

void Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_shouldClose = true;
        }
    }
}