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
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(zero PRIVATE ${SDL_LIBRARY})
target_link_libraries(zero PRIVATE ${Slang_LIBRARY})
target_link_libraries(zero PRIVATE Vulkan::Vulkan)

# set_target_properties(zero PROPERTIES DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# set_runtime_output_directory(${CMAKE_BINARY_DIR}/bin/)