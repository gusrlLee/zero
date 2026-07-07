#ifndef __ZERO_LOG_HEADER__
#define __ZERO_LOG_HEADER__

#include <volk/volk.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
#include <string>

// Vulkan 에러 코드를 문자열로 변환 (대표적인 에러만 추가)
inline const char* getVulkanResultString(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    default: return "UNKNOWN_VULKAN_ERROR";
    }
}

// 실제 체크를 수행하는 인라인 함수
static inline void chk_impl(VkResult result, const char* function, const char* file, int line) {
    if (result != VK_SUCCESS) {
        std::string errorMsg = std::string("[Vulkan Error] ") + getVulkanResultString(result) +
            "\n  File: " + file +
            "\n  Line: " + std::to_string(line) +
            "\n  Call: " + function;

        std::cerr << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }
}

static inline void chk_impl(bool result, const char* function, const char* file, int line) {
    if (!result) {
        std::string errorMsg = std::string("[Error] Call failed (SDL Error: ") + SDL_GetError() + ")" +
            "\n  File: " + file +
            "\n  Line: " + std::to_string(line) +
            "\n  Call: " + function;

        std::cerr << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }
}

#define CHK(x) chk_impl((x), #x, __FILE__, __LINE__)

#endif