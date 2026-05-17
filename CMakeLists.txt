cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
project(zero LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_ARCHITECTURES native)

if(NOT DEFINED OptiX_INSTALL_DIR)
    set(OptiX_INSTALL_DIR "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0" CACHE PATH "Path to OptiX SDK")
endif()

message(STATUS "---------------------------------------------------")
message(STATUS ">> Current CUDA Architectures: ${CMAKE_CUDA_ARCHITECTURES}")
message(STATUS ">> CUDA Compiler Version: ${CMAKE_CUDA_COMPILER_VERSION}")
message(STATUS ">> Using OptiX SDK at: ${OptiX_INSTALL_DIR}")
message(STATUS "---------------------------------------------------")

add_executable(zero src/main.cu)
target_include_directories(zero PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(zero PUBLIC ${CMAKE_SOURCE_DIR}/lib)