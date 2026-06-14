cmake_minimum_required(VERSION 3.30)
project(zero LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# set up Vulkan C++ module (enabled when ENABLE_CPP20_MODULE=ON)
# add_library(VulkanCppModule)
# add_library(Vulkan::cppm ALIAS VulkanCppModule)

# target_compile_definitions(VulkanCppModule PUBLIC
#         VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
#         VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
# )

# target_include_directories(VulkanCppModule PRIVATE "${Vulkan_INCLUDE_DIR}")
# target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)
# set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)
# target_sources(VulkanCppModule
#         PUBLIC
#         FILE_SET cxx_modules TYPE CXX_MODULES
#         BASE_DIRS "${Vulkan_INCLUDE_DIR}"
#         FILES "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
# )

add_subdirectory(ext)

add_executable(
    ${PROJECT_NAME}
    src/main.cpp
    src/vk_gpu.hpp
    src/vk_gpu.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/ext)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)