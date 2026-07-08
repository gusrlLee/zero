cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(zero CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT IS_DIRECTORY $ENV{VULKAN_SDK})
    message(WARNING "Failed to find Vulkan SDK.")
else()
    message(STATUS "Vulkan SDK path: $ENV{VULKAN_SDK}")
endif()

include_directories(SYSTEM $ENV{VULKAN_SDK}/include)
find_package(Vulkan REQUIRED)

find_library(SDL_LIBRARY NAMES SDL3 HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
find_library(Slang_LIBRARY NAMES slang slang-compiler HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)

if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR -DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR -DVK_USE_PLATFORM_XCB_KHR)
endif()

add_executable(${PROJECT_NAME}
    src/main.cpp
    
    src/core/Window.h
    src/core/Window.cpp
    src/core/ZeroApp.h
    src/core/ZeroApp.cpp
    src/core/VulkanContext.h
    src/core/VulkanContext.cpp
    src/core/Swapchain.h
    src/core/Swapchain.cpp
    src/core/Buffer.h
    src/core/Buffer.cpp

    src/renderer/IRenderer.h
    src/renderer/ShaderCompiler.h
    src/renderer/ShaderCompiler.cpp
    src/renderer/Texture.h
    src/renderer/Texture.cpp
    src/renderer/DescriptorManager.h
    src/renderer/DescriptorManager.cpp

    src/renderer/raster/PbrRenderer.h
    src/renderer/raster/PbrRenderer.cpp

    src/scene/Camera.h
    src/scene/Camera.cpp
    src/scene/Mesh.h
    src/scene/Mesh.cpp

    src/util/Log.h
    src/util/TinyGltfImpl.cpp

    shaders/raster/shader.slang
)

target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/vender)

target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE ${Slang_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

# set_target_properties(zero PROPERTIES DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# set_runtime_output_directory(${CMAKE_BINARY_DIR}/bin/)