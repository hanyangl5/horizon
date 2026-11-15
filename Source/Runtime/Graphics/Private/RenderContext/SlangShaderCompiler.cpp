#include "Graphics/RenderContext.h"

#include "SlangSession.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "Core/IThread.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <stdio.h>
#include <string.h>

#include "Core/IMemory.h" //NOTE: this should be the last include in a .cpp

namespace hz
{
namespace
{
// File layout version only; anything that changes emitted DXIL is covered by the cache key.
constexpr uint32_t kShaderCacheVersion = 1;
constexpr uint32_t kShaderCacheMagic = 0x43535A48; // 'HZSC'

// Cacheable stages, in record order.
constexpr ShaderStage kCachedStages[] = { ShaderStageVert, ShaderStageFrag, ShaderStageComp };
constexpr SlangStage  kSlangStages[] = { SLANG_STAGE_VERTEX, SLANG_STAGE_FRAGMENT, SLANG_STAGE_COMPUTE };
constexpr uint32_t    kCachedStageCount = sizeof(kCachedStages) / sizeof(kCachedStages[0]);

struct ShaderCacheHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t stages;
    uint32_t stageCount;
};

struct ShaderCacheStageHeader
{
    uint32_t stage;
    uint32_t entryPointSize;
    uint32_t byteCodeSize;
};

CallOnceGuard          gSlangSessionGuard = INIT_CALL_ONCE_GUARD;
slang::IGlobalSession* gSlangSession = nullptr;

void initSlangGlobalSession()
{
    if (SLANG_FAILED(slang::createGlobalSession(&gSlangSession)))
    {
        gSlangSession = nullptr;
        LOGF(Error, "Slang initialization failed");
    }
}

static bool checkSlangResult(SlangResult result, const char* operation, const Slang::ComPtr<ISlangBlob>& diagnostics = {})
{
    if (diagnostics && diagnostics->getBufferSize())
        LOGF(SLANG_FAILED(result) ? Error : Warning, "Slang %s: %s", operation, (const char*)diagnostics->getBufferPointer());
    else if (SLANG_FAILED(result))
        LOGF(Error, "Slang %s failed (0x%08x)", operation, (uint32_t)result);
    return SLANG_SUCCEEDED(result);
}

void hashShaderBytes(uint64_t& hash, const void* Data, size_t Size)
{
    const uint8_t* pBytes = (const uint8_t*)Data;
    for (size_t i = 0; i < Size; ++i)
        hash = (hash ^ pBytes[i]) * UINT64_C(1099511628211);
}

// Everything that changes the generated DXIL participates in the key, so a cache hit never hides a shader edit.
uint64_t getShaderCacheKey(const ShaderSrcDesc& source, ShaderTarget target)
{
    uint64_t                  hash = UINT64_C(14695981039346656037);
    const uint32_t            version = kShaderCacheVersion;
    const ShaderSrcStageDesc* stages[] = { &source.vert, &source.frag, &source.comp };
    hashShaderBytes(hash, &version, sizeof(version));
    // The compiler build is what version-validates emitted DXIL; the driver never sees this cache, so it stays out of the key.
    const char* pBuildTag = spGetBuildTagString();
    hashShaderBytes(hash, pBuildTag ? pBuildTag : "", pBuildTag ? strlen(pBuildTag) + 1 : 1);
    hashShaderBytes(hash, &target, sizeof(target));
    for (uint32_t i = 0; i < kCachedStageCount; ++i)
    {
        if (!(source.stages & kCachedStages[i]))
            continue;
        hashShaderBytes(hash, &kCachedStages[i], sizeof(kCachedStages[i]));
        hashShaderBytes(hash, stages[i]->pByteCode, stages[i]->byteCodeSize);
        hashShaderBytes(hash, stages[i]->pEntryPoint, strlen(stages[i]->pEntryPoint) + 1);
    }
    return hash;
}

void getShaderCacheFile(uint64_t key, char* pFileName, size_t Size)
{
    snprintf(pFileName, Size, "%016llx.shadercache", (unsigned long long)key);
}

// Owns the cache file bytes until the caller takes them; every early return frees.
struct ShaderCacheFile
{
    ~ShaderCacheFile() { MemDeallocate(pBytes); }

    uint8_t* release()
    {
        uint8_t* pResult = pBytes;
        pBytes = nullptr;
        return pResult;
    }

    uint8_t* pBytes = nullptr;
    size_t   Size = 0;
};

bool readShaderCacheFile(uint64_t key, ShaderCacheFile& file)
{
    char fileName[64] = {};
    getShaderCacheFile(key, fileName, sizeof(fileName));
    FileStream stream = {};
    if (!OpenStreamFromPath(DirectoryShaderBinaries, fileName, FileModeRead, &stream))
        return false;
    const ssize_t fileSize = GetStreamFileSize(&stream);
    file.pBytes = fileSize > 0 ? (uint8_t*)MemAllocate((size_t)fileSize) : nullptr;
    const bool read = file.pBytes && ReadFromStream(&stream, file.pBytes, (size_t)fileSize) == (size_t)fileSize;
    CloseStream(&stream);
    file.Size = read ? (size_t)fileSize : 0;
    return read;
}

bool loadShaderCache(uint64_t key, const ShaderSrcDesc& source, BinaryShaderDesc& binary, void*& pStorage)
{
    ShaderCacheFile file;
    if (!readShaderCacheFile(key, file) || file.Size <= sizeof(ShaderCacheHeader))
        return false;

    const ShaderCacheHeader* pHeader = (const ShaderCacheHeader*)file.pBytes;
    if (pHeader->magic != kShaderCacheMagic || pHeader->version != kShaderCacheVersion ||
        pHeader->stages != (uint32_t)source.stages || pHeader->stageCount > kCachedStageCount)
        return false;

    BinaryShaderDesc          result = { .stages = (ShaderStage)pHeader->stages };
    BinaryShaderStageDesc*    stageDescs[] = { &result.vert, &result.frag, &result.comp };
    const ShaderSrcStageDesc* sourceStages[] = { &source.vert, &source.frag, &source.comp };
    size_t                    offset = sizeof(ShaderCacheHeader);
    for (uint32_t i = 0; i < pHeader->stageCount; ++i)
    {
        if (offset + sizeof(ShaderCacheStageHeader) > file.Size)
            return false;
        const ShaderCacheStageHeader* pStage = (const ShaderCacheStageHeader*)(file.pBytes + offset);
        offset += sizeof(ShaderCacheStageHeader);
        if (!pStage->entryPointSize || !pStage->byteCodeSize || offset + pStage->entryPointSize + pStage->byteCodeSize > file.Size)
            return false;
        char* pEntryPoint = (char*)(file.pBytes + offset);
        if (pEntryPoint[pStage->entryPointSize - 1] != '\0')
            return false;
        offset += pStage->entryPointSize;
        void* pByteCode = file.pBytes + offset;
        offset += pStage->byteCodeSize;
        for (uint32_t stageIndex = 0; stageIndex < kCachedStageCount; ++stageIndex)
        {
            if (kCachedStages[stageIndex] != (ShaderStage)pStage->stage)
                continue;
            stageDescs[stageIndex]->pName = sourceStages[stageIndex]->pName;
            stageDescs[stageIndex]->pEntryPoint = pEntryPoint;
            stageDescs[stageIndex]->pByteCode = pByteCode;
            stageDescs[stageIndex]->byteCodeSize = pStage->byteCodeSize;
        }
    }
    // A truncated or foreign file must not turn into a null entry point inside addShaderBinary.
    for (uint32_t i = 0; i < kCachedStageCount; ++i)
    {
        if ((result.stages & kCachedStages[i]) && (!stageDescs[i]->pEntryPoint || !stageDescs[i]->pByteCode))
            return false;
    }
    binary = result;
    pStorage = file.release();
    return true;
}

void storeShaderCache(uint64_t key, const BinaryShaderDesc& binary)
{
    const BinaryShaderStageDesc* stageDescs[] = { &binary.vert, &binary.frag, &binary.comp };
    uint32_t                     stageCount = 0;
    for (uint32_t i = 0; i < kCachedStageCount; ++i)
        stageCount += (binary.stages & kCachedStages[i]) ? 1u : 0u;

    char       fileName[64] = {};
    FileStream stream = {};
    getShaderCacheFile(key, fileName, sizeof(fileName));
    if (!OpenStreamFromPath(DirectoryShaderBinaries, fileName, FileModeWrite, &stream))
        return;
    const ShaderCacheHeader header = {
        .magic = kShaderCacheMagic, .version = kShaderCacheVersion, .stages = (uint32_t)binary.stages, .stageCount = stageCount
    };
    WriteToStream(&stream, &header, sizeof(header));
    for (uint32_t i = 0; i < kCachedStageCount; ++i)
    {
        if (!(binary.stages & kCachedStages[i]))
            continue;
        const size_t                 entryPointSize = strlen(stageDescs[i]->pEntryPoint) + 1;
        const ShaderCacheStageHeader stage = { .stage = (uint32_t)kCachedStages[i],
                                               .entryPointSize = (uint32_t)entryPointSize,
                                               .byteCodeSize = stageDescs[i]->byteCodeSize };
        WriteToStream(&stream, &stage, sizeof(stage));
        WriteToStream(&stream, stageDescs[i]->pEntryPoint, entryPointSize);
        WriteToStream(&stream, stageDescs[i]->pByteCode, stageDescs[i]->byteCodeSize);
    }
    CloseStream(&stream);
}
} // namespace

slang::IGlobalSession* getSlangGlobalSession()
{
    CallOnce(&gSlangSessionGuard, initSlangGlobalSession);
    return gSlangSession;
}

Shader* RenderContext::createSlangShader(const ShaderSrcDesc& input)
{
    slang::IGlobalSession* pGlobalSession = getSlangGlobalSession();
    if (!pGlobalSession)
        return nullptr;

    if (pRenderer->shaderTarget < SHADER_TARGET_6_0)
    {
        LOGF(Error, "Slang DXIL requires Shader Model 6.0 or newer");
        return nullptr;
    }

    const uint64_t key = getShaderCacheKey(input, (ShaderTarget)pRenderer->shaderTarget);
    BinaryShaderDesc cached = {};
    void*            pCachedBytes = nullptr;
    if (loadShaderCache(key, input, cached, pCachedBytes))
    {
        Shader* shader = nullptr;
        addShaderBinary(pRenderer, &cached, &shader);
        MemDeallocate(pCachedBytes);
        return shader;
    }

    char profile[16] = {};
    snprintf(profile, sizeof(profile), "sm_6_%u", (uint32_t)(pRenderer->shaderTarget - SHADER_TARGET_6_0));
    const slang::CompilerOptionEntry options[] = {
        // Preserve resource names for the existing DXIL reflection and named bindings.
        { .name = slang::CompilerOptionName::NoMangle, .value = { .intValue0 = 1 } },
    };
    const slang::TargetDesc  target = { .format = SLANG_DXIL, .profile = pGlobalSession->findProfile(profile) };
    const slang::SessionDesc desc = {
        .targets = &target,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = options,
        .compilerOptionEntryCount = 1,
    };
    Slang::ComPtr<slang::ISession> session;
    if (!checkSlangResult(pGlobalSession->createSession(desc, session.writeRef()), "session creation"))
        return nullptr;

    const ShaderSrcStageDesc*         sources[] = { &input.vert, &input.frag, &input.comp };
    slang::IModule*                   modules[kCachedStageCount] = {};
    const ShaderSrcStageDesc*         moduleSources[kCachedStageCount] = {};
    Slang::ComPtr<slang::IEntryPoint> entryPoints[kCachedStageCount];
    Slang::ComPtr<ISlangBlob>         diagnostics;
    uint32_t                          moduleCount = 0;
    uint32_t                          entryPointCount = 0;
    for (uint32_t i = 0; i < kCachedStageCount; ++i)
    {
        if (!(input.stages & kCachedStages[i]))
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
        if (!checkSlangResult(modules[moduleIndex]->findAndCheckEntryPoint(sources[i]->pEntryPoint, kSlangStages[i],
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
    Slang::ComPtr<ISlangBlob> bytecode[kCachedStageCount];
    uint32_t                  entryPointIndex = 0;
    for (uint32_t i = 0; i < kCachedStageCount; ++i)
    {
        if (!(input.stages & kCachedStages[i]))
            continue;
        if (!checkSlangResult(linkedProgram->getEntryPointCode(entryPointIndex++, 0, bytecode[i].writeRef(), diagnostics.writeRef()),
                              "DXIL generation", diagnostics))
            return nullptr;
        outputs[i]->pName = sources[i]->pName;
        outputs[i]->pByteCode = (void*)bytecode[i]->getBufferPointer();
        outputs[i]->byteCodeSize = (uint32_t)bytecode[i]->getBufferSize();
        outputs[i]->pEntryPoint = sources[i]->pEntryPoint;
    }
    // Written before the shader is created so a later start skips the front end entirely.
    storeShaderCache(key, binary);

    Shader* shader = nullptr;
    addShaderBinary(pRenderer, &binary, &shader);
    return shader;
}
} // namespace hz
