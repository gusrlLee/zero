#include "core/ZeroApp.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    try {
        // create Zero Application for Vulkan Rendering
        ZeroApp app;

        // Run Program
        app.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}