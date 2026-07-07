#include "core/ZeroApp.h"
#include "core/Window.h"
#include "core/VulkanContext.h"

#include <iostream>

ZeroApp::ZeroApp() : m_isRunning(false) {
    init();
}

ZeroApp::~ZeroApp() {
    cleanup();
}

void ZeroApp::init() {
    // 1. 윈도우 생성
    m_window = std::make_unique<Window>(1980, 1080, "Zero Engine");
    m_vulkanContext = std::make_unique<VulkanContext>(m_window.get());
    
    m_isRunning = true;
    std::cout << "ZeroApp 초기화 완료: Window 생성됨." << std::endl;
}

void ZeroApp::run() {
    mainLoop();
}

void ZeroApp::mainLoop() {
    while (!m_window->shouldClose()) {
        m_window->pollEvents();
        
        // 향후 여기서 렌더러가 호출됩니다.
        // m_vulkanRenderer->drawFrame();
    }
}

void ZeroApp::cleanup() {
    // unique_ptr들은 여기서 ZeroApp이 파괴될 때 
    // 자동으로 순서에 맞게(선언 역순) 파괴됩니다.
    std::cout << "ZeroApp 종료 중..." << std::endl;
}
