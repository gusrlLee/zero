#ifndef __ZERO_SHADER_COMPILER_HEADER__
#define __ZERO_SHADER_COMPILER_HEADER__

#include <volk/volk.h>
#include <string>

// Slang 관련 헤더
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

class VulkanContext;

class ShaderCompiler {
public:
    ShaderCompiler(VulkanContext* context);
    ~ShaderCompiler();

    // 런타임에 .slang 파일을 컴파일하고 SPIR-V를 생성하여 VkShaderModule을 반환
    VkShaderModule compileToShaderModule(const std::string& filePath, const std::string& entryPointName);

private:
    VulkanContext* m_context;
    Slang::ComPtr<slang::IGlobalSession> m_globalSession;
};

#endif