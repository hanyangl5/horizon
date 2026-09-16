/* Copyright (c) 2026 Horizon */

#include "SceneTextureCooker.h"
#include "Core/IToolFileSystem.h"
#include <ThirdParty/cgltf/cgltf.h>
#include <limits.h>
#include <string.h>

#if defined(_WIN32)
#include <DirectXTex.h>
#include <wincodec.h>

class TextureEncoder
{
public:
    TextureEncoder(): comResult(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
        if ((SUCCEEDED(comResult) || comResult == RPC_E_CHANGED_MODE) &&
            SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory))))
            DirectX::SetWICFactory(pFactory);
    }
    ~TextureEncoder()
    {
        if (pFactory)
        {
            DirectX::SetWICFactory(nullptr);
            pFactory->Release();
        }
        if (SUCCEEDED(comResult))
            CoUninitialize();
    }
    TextureEncoder(const TextureEncoder&) = delete;
    TextureEncoder& operator=(const TextureEncoder&) = delete;

    bool encode(const uint8_t* pBytes, size_t size, bool srgb, DirectX::Blob& dds) const;

private:
    HRESULT             comResult;
    IWICImagingFactory* pFactory = nullptr;
};
#endif

#include "Core/IMemory.h"

static uint64_t imageHash(const void* pBytes, size_t size, uint64_t hash = UINT64_C(14695981039346656037))
{
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ ((const uint8_t*)pBytes)[i]) * UINT64_C(1099511628211);
    return hash;
}

bool SceneTextureCooker::readImage(const char* pPath, hz::Array<uint8_t>& bytes) const
{
    FileStream stream = {};
    if (!fsOpenStreamFromPath(sourceDirectory, pPath, FM_READ, &stream))
        return false;
    const ssize_t size = fsGetStreamFileSize(&stream);
    if (size <= 0 || size > INT_MAX)
    {
        fsCloseStream(&stream);
        return false;
    }
    bytes.resize((uint32_t)size);
    const bool read = fsReadFromStream(&stream, bytes.data(), bytes.size()) == bytes.size();
    return fsCloseStream(&stream) && read;
}

static bool decodeImageURI(const char* pURI, hz::Array<uint8_t>& bytes)
{
    const char* pEncoded = nullptr;
    if (strncmp(pURI, "data:image/png;base64,", 22) == 0)
        pEncoded = pURI + 22;
    else if (strncmp(pURI, "data:image/jpeg;base64,", 23) == 0)
        pEncoded = pURI + 23;
    if (!pEncoded)
        return false;
    const size_t length = strlen(pEncoded);
    if (!length || length % 4 || length > INT_MAX)
        return false;
    const size_t padding = (pEncoded[length - 1] == '=') + (pEncoded[length - 2] == '=');
    if (strspn(pEncoded, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/") != length - padding)
        return false;
    bytes.resize((uint32_t)(length / 4 * 3 - padding));
    // Decode through cgltf into the already sized buffer.
    cgltf_options options = {};
    options.memory_user_data = bytes.data();
    options.memory_alloc = [](void* pUser, cgltf_size) { return pUser; };
    options.memory_free = [](void*, void*) {};
    void* pDecoded = nullptr;
    return cgltf_load_buffer_base64(&options, bytes.size(), pEncoded, &pDecoded) == cgltf_result_success;
}

bool SceneTextureCooker::writeDDS(const char* pPath, const void* pBytes, size_t size) const
{
    char temporary[FS_MAX_PATH];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", pPath) >= (int)sizeof(temporary) ||
        !fsCreateDirectory(outputDirectory, "Textures", true))
        return false;
    FileStream stream = {};
    if (!fsOpenStreamFromPath(outputDirectory, temporary, FM_WRITE, &stream))
        return false;
    const bool written = fsWriteToStream(&stream, pBytes, size) == size;
    const bool closed = fsCloseStream(&stream);
    if (!written || !closed)
        return false;
    // Content-addressed output never overwrites a texture used by a previous cook.
    if (fsFileExist(outputDirectory, pPath))
        return fsRemoveFile(outputDirectory, temporary);
    return fsRenameFile(outputDirectory, temporary, pPath);
}

#if defined(_WIN32)

bool TextureEncoder::encode(const uint8_t* pBytes, size_t size, bool srgb, DirectX::Blob& dds) const
{
    if (!pFactory)
        return false;
    DirectX::ScratchImage        image, mipmaps, compressed;
    const DirectX::ScratchImage* pInput = &image;
    HRESULT                      result = DirectX::LoadFromWICMemory(pBytes, size, DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
    if (SUCCEEDED(result))
    {
        if (srgb)
            image.OverrideFormat(DirectX::MakeSRGB(image.GetMetadata().format));
        if (image.GetMetadata().width > 1 || image.GetMetadata().height > 1)
        {
            result = DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmaps);
            pInput = &mipmaps;
        }
    }
    if (SUCCEEDED(result))
        result = DirectX::Compress(pInput->GetImages(), pInput->GetImageCount(), pInput->GetMetadata(),
                                   srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM, DirectX::TEX_COMPRESS_DEFAULT,
                                   DirectX::TEX_THRESHOLD_DEFAULT, compressed);
    if (SUCCEEDED(result))
        result = DirectX::SaveToDDSMemory(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(),
                                          DirectX::DDS_FLAGS_FORCE_DX10_EXT, dds);
    if (FAILED(result))
        LOGF(eERROR, "DirectXTex texture cook failed (HRESULT 0x%08x)", (unsigned)result);
    return SUCCEEDED(result);
}
#endif

SceneTextureCooker::SceneTextureCooker(ResourceDirectory sourceDirectory, ResourceDirectory outputDirectory, const char* pSourceFile):
    sourceDirectory(sourceDirectory), outputDirectory(outputDirectory)
{
    fsGetParentPath(pSourceFile, sourceParent);
}

bool SceneTextureCooker::cook(const cgltf_image& image, bool srgb, CookedSceneTexture& output) const
{
    output.srgb = srgb;
    hz::Array<uint8_t> bytes;
    const uint8_t*     pBytes = nullptr;
    size_t             size = 0;
    if (image.uri && strncmp(image.uri, "data:", 5) != 0)
    {
        if (strstr(image.uri, ":") || image.uri[0] == '/' || strchr(image.uri, '\\') ||
            strlen(sourceParent) + strlen(image.uri) + 2 >= FS_MAX_PATH)
            return false;
        fsAppendPathComponent(sourceParent, image.uri, output.source);
        const char* pExtension = strrchr(image.uri, '.');
        if (pExtension && (stricmp(pExtension, ".dds") == 0 || stricmp(pExtension, ".ktx") == 0))
        {
            snprintf(output.path, sizeof(output.path), "%s", output.source);
            output.signature = imageHash(output.source, strlen(output.source));
            return fsFileExist(sourceDirectory, output.path);
        }
        if (!pExtension || (stricmp(pExtension, ".png") != 0 && stricmp(pExtension, ".jpg") != 0 && stricmp(pExtension, ".jpeg") != 0) ||
            !readImage(output.source, bytes))
            return false;
        pBytes = bytes.data();
        size = bytes.size();
    }
    else if (image.uri)
    {
        if (!decodeImageURI(image.uri, bytes))
            return false;
        pBytes = bytes.data();
        size = bytes.size();
    }
    else if (image.buffer_view)
    {
        const cgltf_buffer_view& view = *image.buffer_view;
        if (!view.buffer || !view.buffer->data || view.offset > view.buffer->size || view.size > view.buffer->size - view.offset)
            return false;
        pBytes = (const uint8_t*)view.buffer->data + view.offset;
        size = view.size;
    }
    if (!pBytes || !size || size > INT_MAX)
        return false;
    output.signature = imageHash(pBytes, size);
#if defined(_WIN32)
    const uint32_t settings[] = { 1, (uint32_t)(srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM) };
    const uint64_t hash = imageHash(settings, sizeof(settings), output.signature);
    snprintf(output.path, sizeof(output.path), "Textures/%016llx.dds", (unsigned long long)hash);
    output.cooked = true;
    if (output.source[0])
        output.signature = imageHash(output.source, strlen(output.source));
    if (fsFileExist(outputDirectory, output.path))
        return true;
    DirectX::Blob        dds;
    const TextureEncoder encoder;
    if (!encoder.encode(pBytes, size, srgb, dds))
        return false;
    const bool written = writeDDS(output.path, dds.GetBufferPointer(), dds.GetBufferSize());
#else
    LOGF(eERROR, "PNG/JPEG scene texture cooking requires DirectXTex on Windows");
    const bool written = false;
#endif
    return written;
}
