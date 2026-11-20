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
constexpr ShaderStage kCachedStages[] = { SHADER_STAGE_VERT, SHADER_STAGE_FRAG, SHADER_STAGE_COMP };
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

void InitSlangGlobalSession()
{
    if (SLANG_FAILED(slang::createGlobalSession(&gSlangSession)))
    {
        gSlangSession = nullptr;
        LOGF(Error, "Slang initialization failed");
    }
}

static bool CheckSlangResult(SlangResult ResultValue, const char* Operation, const Slang::ComPtr<ISlangBlob>& Diagnostics = {})
{
    if (Diagnostics && Diagnostics->getBufferSize())
        LOGF(SLANG_FAILED(ResultValue) ? Error : Warning, "Slang %s: %s", Operation, (const char*)Diagnostics->getBufferPointer());
    else if (SLANG_FAILED(ResultValue))
        LOGF(Error, "Slang %s failed (0x%08x)", Operation, (uint32_t)ResultValue);
    return SLANG_SUCCEEDED(ResultValue);
}

void HashShaderBytes(uint64_t& Hash, const void* Data, size_t Size)
{
    const uint8_t* Bytes = (const uint8_t*)Data;
    for (size_t I = 0; I < Size; ++I)
    {
        Hash = (Hash ^ Bytes[I]) * UINT64_C(1099511628211);
    }
}

// Everything that changes the generated DXIL participates in the key, so a cache hit never hides a shader edit.
uint64_t GetShaderCacheKey(const ShaderSrcDesc& Source, ShaderTarget Target)
{
    uint64_t                  Hash = UINT64_C(14695981039346656037);
    const uint32_t            Version = kShaderCacheVersion;
    const ShaderSrcStageDesc* Stages[] = { &Source.vert, &Source.frag, &Source.comp };
    HashShaderBytes(Hash, &Version, sizeof(Version));
    // The compiler build is what version-validates emitted DXIL; the driver never sees this cache, so it stays out of the key.
    const char* BuildTag = spGetBuildTagString();
    HashShaderBytes(Hash, BuildTag ? BuildTag : "", BuildTag ? strlen(BuildTag) + 1 : 1);
    HashShaderBytes(Hash, &Target, sizeof(Target));
    for (uint32_t I = 0; I < kCachedStageCount; ++I)
    {
        if (!(Source.stages & kCachedStages[I]))
        {
            continue;
        }
        HashShaderBytes(Hash, &kCachedStages[I], sizeof(kCachedStages[I]));
        HashShaderBytes(Hash, Stages[I]->pByteCode, Stages[I]->byteCodeSize);
        HashShaderBytes(Hash, Stages[I]->pEntryPoint, strlen(Stages[I]->pEntryPoint) + 1);
    }
    return Hash;
}

void GetShaderCacheFile(uint64_t Key, char* FileNamePointer, size_t Size)
{
    snprintf(FileNamePointer, Size, "%016llx.shadercache", (unsigned long long)Key);
}

// Owns the cache file bytes until the caller takes them; every early return frees.
struct ShaderCacheFile
{
    ~ShaderCacheFile() { MemDeallocate(pBytes); }

    uint8_t* Release()
    {
        uint8_t* ResultPointer = pBytes;
        pBytes = nullptr;
        return ResultPointer;
    }

    uint8_t* pBytes = nullptr;
    size_t   Size = 0;
};

bool ReadShaderCacheFile(uint64_t Key, ShaderCacheFile& File)
{
    char FileNameValue[64] = {};
    GetShaderCacheFile(Key, FileNameValue, sizeof(FileNameValue));
    FileStream Stream = {};
    if (!OpenStreamFromPath(DirectoryShaderBinaries, FileNameValue, FileModeRead, &Stream))
    {
        return false;
    }
    const ssize_t FileSize = GetStreamFileSize(&Stream);
    File.pBytes = FileSize > 0 ? (uint8_t*)MemAllocate((size_t)FileSize) : nullptr;
    const bool Read = File.pBytes && ReadFromStream(&Stream, File.pBytes, (size_t)FileSize) == (size_t)FileSize;
    CloseStream(&Stream);
    File.Size = Read ? (size_t)FileSize : 0;
    return Read;
}

bool LoadShaderCache(uint64_t Key, const ShaderSrcDesc& Source, BinaryShaderDesc& Binary, void*& Storage)
{
    ShaderCacheFile File;
    if (!ReadShaderCacheFile(Key, File) || File.Size <= sizeof(ShaderCacheHeader))
    {
        return false;
    }

    const ShaderCacheHeader* HeaderPointer = (const ShaderCacheHeader*)File.pBytes;
    if (HeaderPointer->magic != kShaderCacheMagic || HeaderPointer->version != kShaderCacheVersion || HeaderPointer->stages != (uint32_t)Source.stages ||
        HeaderPointer->stageCount > kCachedStageCount)
    {
        return false;
    }

    BinaryShaderDesc          ResultValue = { .stages = (ShaderStage)HeaderPointer->stages };
    BinaryShaderStageDesc*    StageDescs[] = { &ResultValue.vert, &ResultValue.frag, &ResultValue.comp };
    const ShaderSrcStageDesc* SourceStages[] = { &Source.vert, &Source.frag, &Source.comp };
    size_t                    Offset = sizeof(ShaderCacheHeader);
    for (uint32_t I = 0; I < HeaderPointer->stageCount; ++I)
    {
        if (Offset + sizeof(ShaderCacheStageHeader) > File.Size)
        {
            return false;
        }
        const ShaderCacheStageHeader* StagePointer = (const ShaderCacheStageHeader*)(File.pBytes + Offset);
        Offset += sizeof(ShaderCacheStageHeader);
        if (!StagePointer->entryPointSize || !StagePointer->byteCodeSize || Offset + StagePointer->entryPointSize + StagePointer->byteCodeSize > File.Size)
        {
            return false;
        }
        char* EntryPoint = (char*)(File.pBytes + Offset);
        if (EntryPoint[StagePointer->entryPointSize - 1] != '\0')
        {
            return false;
        }
        Offset += StagePointer->entryPointSize;
        void* ByteCode = File.pBytes + Offset;
        Offset += StagePointer->byteCodeSize;
        for (uint32_t StageIndex = 0; StageIndex < kCachedStageCount; ++StageIndex)
        {
            if (kCachedStages[StageIndex] != (ShaderStage)StagePointer->stage)
            {
                continue;
            }
            StageDescs[StageIndex]->Name = SourceStages[StageIndex]->Name;
            StageDescs[StageIndex]->pEntryPoint = EntryPoint;
            StageDescs[StageIndex]->pByteCode = ByteCode;
            StageDescs[StageIndex]->byteCodeSize = StagePointer->byteCodeSize;
        }
    }
    // A truncated or foreign file must not turn into a null entry point inside addShaderBinary.
    for (uint32_t I = 0; I < kCachedStageCount; ++I)
    {
        if ((ResultValue.stages & kCachedStages[I]) && (!StageDescs[I]->pEntryPoint || !StageDescs[I]->pByteCode))
        {
            return false;
        }
    }
    Binary = ResultValue;
    Storage = File.Release();
    return true;
}

void StoreShaderCache(uint64_t Key, const BinaryShaderDesc& Binary)
{
    const BinaryShaderStageDesc* StageDescs[] = { &Binary.vert, &Binary.frag, &Binary.comp };
    uint32_t                     StageCount = 0;
    for (uint32_t I = 0; I < kCachedStageCount; ++I)
    {
        StageCount += (Binary.stages & kCachedStages[I]) ? 1u : 0u;
    }

    char       FileNameValue[64] = {};
    FileStream Stream = {};
    GetShaderCacheFile(Key, FileNameValue, sizeof(FileNameValue));
    if (!OpenStreamFromPath(DirectoryShaderBinaries, FileNameValue, FileModeWrite, &Stream))
    {
        return;
    }
    const ShaderCacheHeader HeaderValue = { .magic = kShaderCacheMagic,
                                       .version = kShaderCacheVersion,
                                       .stages = (uint32_t)Binary.stages,
                                       .stageCount = StageCount };
    WriteToStream(&Stream, &HeaderValue, sizeof(HeaderValue));
    for (uint32_t I = 0; I < kCachedStageCount; ++I)
    {
        if (!(Binary.stages & kCachedStages[I]))
        {
            continue;
        }
        const size_t                 EntryPointSize = strlen(StageDescs[I]->pEntryPoint) + 1;
        const ShaderCacheStageHeader StageValue = { .stage = (uint32_t)kCachedStages[I],
                                               .entryPointSize = (uint32_t)EntryPointSize,
                                               .byteCodeSize = StageDescs[I]->byteCodeSize };
        WriteToStream(&Stream, &StageValue, sizeof(StageValue));
        WriteToStream(&Stream, StageDescs[I]->pEntryPoint, EntryPointSize);
        WriteToStream(&Stream, StageDescs[I]->pByteCode, StageDescs[I]->byteCodeSize);
    }
    CloseStream(&Stream);
}
} // namespace

slang::IGlobalSession* GetSlangGlobalSession()
{
    CallOnce(&gSlangSessionGuard, InitSlangGlobalSession);
    return gSlangSession;
}

Shader* RenderContext::CreateSlangShader(const ShaderSrcDesc& Input)
{
    slang::IGlobalSession* GlobalSession = GetSlangGlobalSession();
    if (!GlobalSession)
    {
        return nullptr;
    }

    if (pRenderer->shaderTarget < SHADER_TARGET_6_0)
    {
        LOGF(Error, "Slang DXIL requires Shader Model 6.0 or newer");
        return nullptr;
    }

    const uint64_t   Key = GetShaderCacheKey(Input, (ShaderTarget)pRenderer->shaderTarget);
    BinaryShaderDesc Cached = {};
    void*            CachedBytes = nullptr;
    if (LoadShaderCache(Key, Input, Cached, CachedBytes))
    {
        Shader* ShaderObject = nullptr;
        addShaderBinary(pRenderer, &Cached, &ShaderObject);
        MemDeallocate(CachedBytes);
        return ShaderObject;
    }

    char Profile[16] = {};
    snprintf(Profile, sizeof(Profile), "sm_6_%u", (uint32_t)(pRenderer->shaderTarget - SHADER_TARGET_6_0));
    const slang::CompilerOptionEntry Options[] = {
        // Preserve resource names for the existing DXIL reflection and named bindings.
        { .name = slang::CompilerOptionName::NoMangle, .value = { .intValue0 = 1 } },
    };
    const slang::TargetDesc  Target = { .format = SLANG_DXIL, .profile = GlobalSession->findProfile(Profile) };
    const slang::SessionDesc Desc = {
        .targets = &Target,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = Options,
        .compilerOptionEntryCount = 1,
    };
    Slang::ComPtr<slang::ISession> Session;
    if (!CheckSlangResult(GlobalSession->createSession(Desc, Session.writeRef()), "session creation"))
    {
        return nullptr;
    }

    const ShaderSrcStageDesc*         Sources[] = { &Input.vert, &Input.frag, &Input.comp };
    slang::IModule*                   Modules[kCachedStageCount] = {};
    const ShaderSrcStageDesc*         ModuleSources[kCachedStageCount] = {};
    Slang::ComPtr<slang::IEntryPoint> EntryPoints[kCachedStageCount];
    Slang::ComPtr<ISlangBlob>         Diagnostics;
    uint32_t                          ModuleCount = 0;
    uint32_t                          EntryPointCount = 0;
    for (uint32_t I = 0; I < kCachedStageCount; ++I)
    {
        if (!(Input.stages & kCachedStages[I]))
        {
            continue;
        }
        uint32_t ModuleIndex = 0;
        while (ModuleIndex < ModuleCount && (ModuleSources[ModuleIndex]->pByteCode != Sources[I]->pByteCode ||
                                             ModuleSources[ModuleIndex]->byteCodeSize != Sources[I]->byteCodeSize))
            ++ModuleIndex;
        if (ModuleIndex == ModuleCount)
        {
            char ModuleName[32] = {};
            snprintf(ModuleName, sizeof(ModuleName), "HorizonShader%u", ModuleCount);
            Slang::ComPtr<ISlangBlob> Source;
            Source.attach(slang_createBlob(Sources[I]->pByteCode, Sources[I]->byteCodeSize));
            Modules[ModuleIndex] = Session->loadModuleFromSource(ModuleName, Sources[I]->Name, Source, Diagnostics.writeRef());
            if (!CheckSlangResult(Modules[ModuleIndex] ? SLANG_OK : SLANG_FAIL, "module loading", Diagnostics))
                return nullptr;
            ModuleSources[ModuleCount++] = Sources[I];
        }
        if (!CheckSlangResult(Modules[ModuleIndex]->findAndCheckEntryPoint(Sources[I]->pEntryPoint, kSlangStages[I],
                                                                           EntryPoints[EntryPointCount].writeRef(), Diagnostics.writeRef()),
                              "entry point lookup", Diagnostics))
        {
            return nullptr;
        }
        ++EntryPointCount;
    }

    slang::IComponentType* Components[6] = {};
    for (uint32_t I = 0; I < ModuleCount; ++I)
        Components[I] = Modules[I];
    for (uint32_t I = 0; I < EntryPointCount; ++I)
        Components[ModuleCount + I] = EntryPoints[I];
    Slang::ComPtr<slang::IComponentType> Program;
    Slang::ComPtr<slang::IComponentType> LinkedProgram;
    if (!CheckSlangResult(
            Session->createCompositeComponentType(Components, ModuleCount + EntryPointCount, Program.writeRef(), Diagnostics.writeRef()),
            "program composition", Diagnostics) ||
        !CheckSlangResult(Program->link(LinkedProgram.writeRef(), Diagnostics.writeRef()), "linking", Diagnostics))
        return nullptr;

    BinaryShaderDesc          Binary = { .stages = Input.stages };
    BinaryShaderStageDesc*    Outputs[] = { &Binary.vert, &Binary.frag, &Binary.comp };
    Slang::ComPtr<ISlangBlob> Bytecode[kCachedStageCount];
    uint32_t                  EntryPointIndex = 0;
    for (uint32_t I = 0; I < kCachedStageCount; ++I)
    {
        if (!(Input.stages & kCachedStages[I]))
        {
            continue;
        }
        if (!CheckSlangResult(LinkedProgram->getEntryPointCode(EntryPointIndex++, 0, Bytecode[I].writeRef(), Diagnostics.writeRef()),
                              "DXIL generation", Diagnostics))
            return nullptr;
        Outputs[I]->Name = Sources[I]->Name;
        Outputs[I]->pByteCode = (void*)Bytecode[I]->getBufferPointer();
        Outputs[I]->byteCodeSize = (uint32_t)Bytecode[I]->getBufferSize();
        Outputs[I]->pEntryPoint = Sources[I]->pEntryPoint;
    }
    // Written before the shader is created so a later start skips the front end entirely.
    StoreShaderCache(Key, Binary);

    Shader* ShaderObject = nullptr;
    addShaderBinary(pRenderer, &Binary, &ShaderObject);
    return ShaderObject;
}
} // namespace hz
