#include "Graphics/RenderContext.h"

#include "Core/ILog.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <stdio.h>

namespace hz
{

static bool checkSlangResult(SlangResult result, const char* operation, const Slang::ComPtr<ISlangBlob>& diagnostics = {})
{
    if (diagnostics && diagnostics->getBufferSize())
        LOGF(SLANG_FAILED(result) ? eERROR : eWARNING, "Slang %s: %s", operation, (const char*)diagnostics->getBufferPointer());
    else if (SLANG_FAILED(result))
        LOGF(eERROR, "Slang %s failed (0x%08x)", operation, (uint32_t)result);
    return SLANG_SUCCEEDED(result);
}

Shader* RenderContext::createSlangShader(const ShaderSrcDesc& input)
{
    if (!pSlangSession && !checkSlangResult(slang::createGlobalSession(&pSlangSession), "initialization"))
        return nullptr;

    if (pRenderer->shaderTarget < SHADER_TARGET_6_0)
    {
        LOGF(eERROR, "Slang DXIL requires Shader Model 6.0 or newer");
        return nullptr;
    }
    char profile[16] = {};
    snprintf(profile, sizeof(profile), "sm_6_%u", (uint32_t)(pRenderer->shaderTarget - SHADER_TARGET_6_0));
    const slang::CompilerOptionEntry options[] = {
        // Preserve resource names for the existing DXIL reflection and named bindings.
        { .name = slang::CompilerOptionName::NoMangle, .value = { .intValue0 = 1 } },
    };
    const slang::TargetDesc  target = { .format = SLANG_DXIL, .profile = pSlangSession->findProfile(profile) };
    const slang::SessionDesc desc = {
        .targets = &target,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = options,
        .compilerOptionEntryCount = 1,
    };
    Slang::ComPtr<slang::ISession> session;
    if (!checkSlangResult(pSlangSession->createSession(desc, session.writeRef()), "session creation"))
        return nullptr;

    const ShaderSrcStageDesc*         sources[] = { &input.vert, &input.frag, &input.comp };
    const ShaderStage                 stages[] = { SHADER_STAGE_VERT, SHADER_STAGE_FRAG, SHADER_STAGE_COMP };
    const SlangStage                  slangStages[] = { SLANG_STAGE_VERTEX, SLANG_STAGE_FRAGMENT, SLANG_STAGE_COMPUTE };
    slang::IModule*                   modules[3] = {};
    const ShaderSrcStageDesc*         moduleSources[3] = {};
    Slang::ComPtr<slang::IEntryPoint> entryPoints[3];
    Slang::ComPtr<ISlangBlob>         diagnostics;
    uint32_t                          moduleCount = 0;
    uint32_t                          entryPointCount = 0;
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (!(input.stages & stages[i]))
            continue;
        uint32_t moduleIndex = 0;
        while (moduleIndex < moduleCount && (moduleSources[moduleIndex]->pByteCode != sources[i]->pByteCode ||
                                             moduleSources[moduleIndex]->byteCodeSize != sources[i]->byteCodeSize))
            ++moduleIndex;
        if (moduleIndex == moduleCount)
        {
            char moduleName[32] = {};
            snprintf(moduleName, sizeof(moduleName), "HorizonShader%u", moduleCount);
            Slang::ComPtr<ISlangBlob> source;
            source.attach(slang_createBlob(sources[i]->pByteCode, sources[i]->byteCodeSize));
            modules[moduleIndex] = session->loadModuleFromSource(moduleName, sources[i]->pName, source, diagnostics.writeRef());
            if (!checkSlangResult(modules[moduleIndex] ? SLANG_OK : SLANG_FAIL, "module loading", diagnostics))
                return nullptr;
            moduleSources[moduleCount++] = sources[i];
        }
        if (!checkSlangResult(modules[moduleIndex]->findAndCheckEntryPoint(sources[i]->pEntryPoint, slangStages[i],
                                                                           entryPoints[entryPointCount].writeRef(), diagnostics.writeRef()),
                              "entry point lookup", diagnostics))
            return nullptr;
        ++entryPointCount;
    }

    slang::IComponentType* components[6] = {};
    for (uint32_t i = 0; i < moduleCount; ++i)
        components[i] = modules[i];
    for (uint32_t i = 0; i < entryPointCount; ++i)
        components[moduleCount + i] = entryPoints[i];
    Slang::ComPtr<slang::IComponentType> program;
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    if (!checkSlangResult(
            session->createCompositeComponentType(components, moduleCount + entryPointCount, program.writeRef(), diagnostics.writeRef()),
            "program composition", diagnostics) ||
        !checkSlangResult(program->link(linkedProgram.writeRef(), diagnostics.writeRef()), "linking", diagnostics))
        return nullptr;

    BinaryShaderDesc          binary = { .stages = input.stages };
    BinaryShaderStageDesc*    outputs[] = { &binary.vert, &binary.frag, &binary.comp };
    Slang::ComPtr<ISlangBlob> bytecode[3];
    uint32_t                  entryPointIndex = 0;
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (!(input.stages & stages[i]))
            continue;
        if (!checkSlangResult(linkedProgram->getEntryPointCode(entryPointIndex++, 0, bytecode[i].writeRef(), diagnostics.writeRef()),
                              "DXIL generation", diagnostics))
            return nullptr;
        outputs[i]->pName = sources[i]->pName;
        outputs[i]->pByteCode = (void*)bytecode[i]->getBufferPointer();
        outputs[i]->byteCodeSize = (uint32_t)bytecode[i]->getBufferSize();
        outputs[i]->pEntryPoint = sources[i]->pEntryPoint;
    }
    Shader* shader = nullptr;
    addShaderBinary(pRenderer, &binary, &shader);
    return shader;
}

void RenderContext::destroyShaderCompiler()
{
    if (pSlangSession)
    {
        pSlangSession->release();
        pSlangSession = nullptr;
    }
}

} // namespace hz
