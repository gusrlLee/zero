#include "core/Window.h"

#include <iostream>

int main(int argc, char* argv[]) {
    try {
        Window window(1280, 720, "Zero Rendering Engine");

        while (!window.shouldClose()) {
            window.pollEvents();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}