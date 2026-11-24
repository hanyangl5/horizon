/* Copyright (c) 2026 Horizon */

#include "SceneTextureCooker.h"
#include "CookUtilities.h"
#include "Core/IToolFileSystem.h"
#include "Core/ITime.h"
#include <ThirdParty/cgltf/cgltf.h>
#include <limits.h>
#include <string.h>

#if defined(_WIN32)
#include <d3d11.h>
#include <DirectXTex.h>
#include <wincodec.h>

class TextureEncoder
{
public:
    TextureEncoder();
    ~TextureEncoder();
    TextureEncoder(const TextureEncoder&) = delete;
    TextureEncoder& operator=(const TextureEncoder&) = delete;

    bool encode(const uint8_t* pBytes, size_t Size, bool srgb, const char* Name, DirectX::Blob& dds) const;

private:
    HRESULT             comResult;
    IWICImagingFactory* pFactory = nullptr;
    ID3D11Device*       pDevice = nullptr;
};
#endif

#include "Core/IMemory.h"

bool SceneTextureCooker::readImage(const char* pPath, hz::Array<uint8_t>& bytes) const
{
    FileStream stream = {};
    if (!OpenStreamFromPath(sourceDirectory, pPath, FileModeRead, &stream))
    {
        return false;
    }
    const ssize_t Size = GetStreamFileSize(&stream);
    if (Size <= 0 || Size > INT_MAX)
    {
        CloseStream(&stream);
        return false;
    }
    bytes.Resize((uint32_t)Size);
    const bool read = ReadFromStream(&stream, bytes.GetData(), bytes.Size()) == bytes.Size();
    return CloseStream(&stream) && read;
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
    bytes.Resize((uint32_t)(length / 4 * 3 - padding));
    // Decode through cgltf into the already sized buffer.
    cgltf_options options = {};
    options.memory_user_data = bytes.GetData();
    options.memory_alloc = [](void* User, cgltf_size) { return User; };
    options.memory_free = [](void*, void*) {};
    void* pDecoded = nullptr;
    return cgltf_load_buffer_base64(&options, bytes.Size(), pEncoded, &pDecoded) == cgltf_result_success;
}

bool SceneTextureCooker::writeDDS(const char* pPath, const void* pBytes, size_t Size) const
{
    char temporary[FS_MAX_PATH];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", pPath) >= (int)sizeof(temporary) ||
        !MakeDirectory(outputDirectory, "Textures", true))
    {
        return false;
    }
    FileStream stream = {};
    if (!OpenStreamFromPath(outputDirectory, temporary, FileModeWrite, &stream))
    {
        return false;
    }
    const bool written = WriteToStream(&stream, pBytes, Size) == Size;
    const bool closed = CloseStream(&stream);
    if (!written || !closed)
        return false;
    // Content-addressed output never overwrites a texture used by a previous cook.
    if (FileExists(outputDirectory, pPath))
    {
        return RemoveFile(outputDirectory, temporary);
    }
    return RenameFile(outputDirectory, temporary, pPath);
}

#if defined(_WIN32)

TextureEncoder::TextureEncoder(): comResult(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
{
    if ((SUCCEEDED(comResult) || comResult == RPC_E_CHANGED_MODE) &&
        SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory))))
    {
        DirectX::SetWICFactory(pFactory);
    }
    // The DirectCompute BC7 encoder is orders of magnitude faster than the single-threaded CPU encoder.
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &featureLevel, 1, D3D11_SDK_VERSION, &pDevice, nullptr,
                                 nullptr)))
    {
        LOGF(Warning, "No D3D11 hardware device; BC7 textures use the slow CPU encoder");
    }
}

TextureEncoder::~TextureEncoder()
{
    if (pDevice)
    {
        pDevice->Release();
    }
    if (pFactory)
    {
        DirectX::SetWICFactory(nullptr);
        pFactory->Release();
    }
    if (SUCCEEDED(comResult))
    {
        CoUninitialize();
    }
}

bool TextureEncoder::encode(const uint8_t* pBytes, size_t Size, bool srgb, const char* Name, DirectX::Blob& dds) const
{
    if (!pFactory)
        return false;
    DirectX::ScratchImage        image, mipmaps, compressed;
    const DirectX::ScratchImage* pInput = &image;
    HRESULT                      result = DirectX::LoadFromWICMemory(pBytes, Size, DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
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
    {
        DirectX::CompressOptions options = { .threshold = DirectX::TEX_THRESHOLD_DEFAULT,
                                             .alphaWeight = DirectX::TEX_ALPHA_WEIGHT_DEFAULT };
#if defined(_OPENMP)
        options.flags = DirectX::TEX_COMPRESS_PARALLEL;
#endif
        const int64_t start = GetUSec(true);
        LOGF(Info, "Compress '%s': %zux%zu, BC7, %zu mip levels (%s)", Name, pInput->GetMetadata().width, pInput->GetMetadata().height,
             pInput->GetImageCount(),
             pDevice                                          ? "GPU"
             : options.flags & DirectX::TEX_COMPRESS_PARALLEL ? "parallel"
                                                              : "serial");
        size_t     lastProgress = SIZE_MAX;
        const auto progress = [&](size_t current, size_t total)
        {
            // The array overload reports zero-based indices after Each mip.
            if (lastProgress != SIZE_MAX && pInput->GetImageCount() > 1 && current < total)
            {
                ++current;
            }
            current = TF_MIN(current, total);
            if (current != lastProgress)
            {
                LOGF(Info, "Compress '%s': %zu/%zu mip levels", Name, current, total);
            }
            lastProgress = current;
            return true;
        };
        const DXGI_FORMAT Format = srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
        result = pDevice ? DirectX::CompressEx(pDevice, pInput->GetImages(), pInput->GetImageCount(), pInput->GetMetadata(), Format,
                                               options, compressed, progress)
                         : DirectX::CompressEx(pInput->GetImages(), pInput->GetImageCount(), pInput->GetMetadata(), Format, options,
                                               compressed, progress);
        if (SUCCEEDED(result))
        {
            LOGF(Info, "Compressed '%s' in %.2f seconds", Name, (GetUSec(true) - start) / 1000000.0);
        }
    }
    if (SUCCEEDED(result))
        result = DirectX::SaveToDDSMemory(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(),
                                          DirectX::DDS_FLAGS_FORCE_DX10_EXT, dds);
    if (FAILED(result))
        LOGF(Error, "DirectXTex texture cook failed for '%s' (HRESULT 0x%08x)", Name, (unsigned)result);
    return SUCCEEDED(result);
}
#endif

SceneTextureCooker::SceneTextureCooker(ResourceDirectory sourceDirectory, ResourceDirectory outputDirectory, const char* pSourceFile):
    sourceDirectory(sourceDirectory), outputDirectory(outputDirectory)
{
    GetParentPath(pSourceFile, sourceParent);
#if defined(_WIN32)
    pEncoder = MemNew(TextureEncoder);
#endif
}

SceneTextureCooker::~SceneTextureCooker()
{
#if defined(_WIN32)
    MemDelete(pEncoder);
#endif
}

bool SceneTextureCooker::cook(const cgltf_image& image, bool srgb, CookedSceneTexture& output) const
{
    output.srgb = srgb;
    hz::Array<uint8_t> bytes;
    const uint8_t*     pBytes = nullptr;
    size_t             Size = 0;
    if (image.uri && strncmp(image.uri, "data:", 5) != 0)
    {
        if (strstr(image.uri, ":") || image.uri[0] == '/' || strchr(image.uri, '\\') ||
            strlen(sourceParent) + strlen(image.uri) + 2 >= FS_MAX_PATH)
            return false;
        AppendPathComponent(sourceParent, image.uri, output.source);
        const char* pExtension = strrchr(image.uri, '.');
        if (pExtension && (stricmp(pExtension, ".dds") == 0 || stricmp(pExtension, ".ktx") == 0))
        {
            snprintf(output.path, sizeof(output.path), "%s", output.source);
            output.signature = cookHash(output.source, strlen(output.source));
            return FileExists(sourceDirectory, output.path);
        }
        if (!pExtension || (stricmp(pExtension, ".png") != 0 && stricmp(pExtension, ".jpg") != 0 && stricmp(pExtension, ".jpeg") != 0) ||
            !readImage(output.source, bytes))
            return false;
        pBytes = bytes.GetData();
        Size = bytes.Size();
    }
    else if (image.uri)
    {
        if (!decodeImageURI(image.uri, bytes))
            return false;
        pBytes = bytes.GetData();
        Size = bytes.Size();
    }
    else if (image.buffer_view)
    {
        const cgltf_buffer_view& view = *image.buffer_view;
        if (!view.buffer || !view.buffer->data || view.offset > view.buffer->size || view.size > view.buffer->size - view.offset)
            return false;
        pBytes = (const uint8_t*)view.buffer->data + view.offset;
        Size = view.size;
    }
    if (!pBytes || !Size || Size > INT_MAX)
    {
        return false;
    }
    output.signature = cookHash(pBytes, Size);
#if defined(_WIN32)
    const uint32_t settings[] = { 1, (uint32_t)(srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM) };
    const uint64_t Hash = cookHash(settings, sizeof(settings), output.signature);
    snprintf(output.path, sizeof(output.path), "Textures/%016llx.dds", (unsigned long long)Hash);
    output.cooked = true;
    if (output.source[0])
        output.signature = cookHash(output.source, strlen(output.source));
    if (FileExists(outputDirectory, output.path))
    {
        return true;
    }
    DirectX::Blob dds;
    if (!pEncoder->encode(pBytes, Size, srgb, output.source[0] ? output.source : output.path, dds))
    {
        return false;
    }
    const bool written = writeDDS(output.path, dds.GetBufferPointer(), dds.GetBufferSize());
#else
    LOGF(Error, "PNG/JPEG scene texture cooking requires DirectXTex on Windows");
    const bool written = false;
#endif
    return written;
}
