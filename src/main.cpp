#include "core/ZeroApp.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    try {
        // 엔진의 오케스트레이터인 ZeroApp을 생성합니다.
        // 이 안에서 Window, VulkanContext 등이 차례로 초기화됩니다.
        ZeroApp app;

        // 엔진 실행 (내부적으로 메인 루프를 돌립니다)
        app.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}