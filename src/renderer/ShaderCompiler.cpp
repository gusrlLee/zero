#include "renderer/ShaderCompiler.h"
#include "core/VulkanContext.h"
#include "util/Log.h"

#include <iostream>
#include <vector>
#include <array>

ShaderCompiler::ShaderCompiler(VulkanContext* context) : m_context(context) {
    // 1. Slang 글로벌 세션 생성
    if (SLANG_FAILED(slang::createGlobalSession(m_globalSession.writeRef()))) {
        throw std::runtime_error("[ShaderCompiler] Failed to create Slang global session");
    }
}

ShaderCompiler::~ShaderCompiler() {
    // ComPtr이므로 자동 소멸됩니다.
}

VkShaderModule ShaderCompiler::compileToShaderModule(const std::string& filePath, const std::string& entryPointName) {
    // 타겟 설정 (Vulkan 1.3의 SPIR-V 1.4 기준)
    std::array<slang::TargetDesc, 1> slangTargets = {{
        { .format = SLANG_SPIRV, .profile = m_globalSession->findProfile("spirv_1_4") }
    }};

    // 컴파일러 옵션 설정 (SPIR-V 직접 출력)
    std::array<slang::CompilerOptionEntry, 1> slangOptions = {{
        { slang::CompilerOptionName::EmitSpirvDirectly, { slang::CompilerOptionValueKind::Int, 1 } }
    }};

    slang::SessionDesc slangSessionDesc{
        .targets = slangTargets.data(),
        .targetCount = static_cast<SlangInt>(slangTargets.size()),
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR, // 수학 라이브러리(GLM 등) 호환성
        .compilerOptionEntries = slangOptions.data(),
        .compilerOptionEntryCount = static_cast<uint32_t>(slangOptions.size())
    };

    Slang::ComPtr<slang::ISession> slangSession;
    m_globalSession->createSession(slangSessionDesc, slangSession.writeRef());

    // 파일에서 모듈 로드 (에러가 나면 콘솔에 찍기 위해 nullptr 대신 진단 메시지 캡처 가능)
    Slang::ComPtr<slang::IModule> slangModule{ slangSession->loadModuleFromSource("triangle_shader", filePath.c_str(), nullptr, nullptr) };    
    if (!slangModule) {
        throw std::runtime_error("[ShaderCompiler] Failed to load Slang module: " + filePath);
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    slangModule->findEntryPointByName(entryPointName.c_str(), entryPoint.writeRef());

    // 컴파일 프로그램 생성
    slang::IComponentType* componentTypes[] = { slangModule, entryPoint };
    Slang::ComPtr<slang::IComponentType> program;
    slangSession->createCompositeComponentType(componentTypes, 2, program.writeRef());

    // SPIR-V 코드로 변환
    Slang::ComPtr<slang::IBlob> spirvBlob;
    Slang::ComPtr<slang::IBlob> diagnosticBlob; // raw 포인터 대신 ComPtr 사용
    SlangResult res = program->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnosticBlob.writeRef());

    if (diagnosticBlob && diagnosticBlob->getBufferSize() > 0) {
        std::string errorMsg((const char*)diagnosticBlob->getBufferPointer(), diagnosticBlob->getBufferSize());
        std::cerr << "[ShaderCompiler Diagnostic]\n" << errorMsg << std::endl;
    }

    if (SLANG_FAILED(res)) {
        throw std::runtime_error("[ShaderCompiler] Failed to compile Slang code to SPIR-V.");
    }

    // 변환된 SPIR-V를 Vulkan Shader Module로 포장
    VkShaderModuleCreateInfo moduleCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirvBlob->getBufferSize(),
        .pCode = reinterpret_cast<const uint32_t*>(spirvBlob->getBufferPointer())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    CHK(vkCreateShaderModule(m_context->getDevice(), &moduleCI, nullptr, &shaderModule));

    return shaderModule;
}