#pragma once

#include "VKMesh11.h"

#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>

#include <ktx-software/lib/src/gl_format.h>
#include <ktx.h>
struct LoadedTextureData
{
    uint32_t         index = 0;
    ktxTexture1*     ktxTex = nullptr;
    lvk::TextureDesc desc;
};

LoadedTextureData loadTextureData(const char* fileName)
{
    const bool isKTX = endsWith(fileName, ".ktx") || endsWith(fileName, ".KTX");

    if (!isKTX)
    {
        printf("Unable to load not-KTX file %s\n", fileName);
        return {};
    }

    ktxTexture1* ktxTex = nullptr;

    if (!LVK_VERIFY(ktxTexture1_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex) == KTX_SUCCESS))
    {
        LLOGW("Failed to load %s\n", fileName);
        assert(0);
        return {};
    }

    const lvk::Format format = [](uint32_t glInternalFormat)
    {
        switch (glInternalFormat)
        {
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
            return lvk::Format_BC7_RGBA;
        case GL_RGBA8:
            return lvk::Format_RGBA_UN8;
        case GL_RG16F:
            return lvk::Format_RG_F16;
        case GL_RGBA16F:
            return lvk::Format_RGBA_F16;
        case GL_RGBA32F:
            return lvk::Format_RGBA_F32;

        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
            return lvk::Format_ASTC_4x4_RGBA;
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
            return lvk::Format_ASTC_4x4_SRGBA;

        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
            return lvk::Format_ASTC_6x6_RGBA;
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
            return lvk::Format_ASTC_6x6_SRGBA;

        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
            return lvk::Format_ASTC_8x8_RGBA;
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
            return lvk::Format_ASTC_8x8_SRGBA;

        default:
            LLOGW("Unsupported pixel format (%u)\n", glInternalFormat);
        }
        return lvk::Format_Invalid;
    }(ktxTex->glInternalformat);

    return LoadedTextureData{ .ktxTex = ktxTex,
                              .desc = { .type = lvk::TextureType_2D,
                                        .format = format,
                                        .dimensions = { ktxTex->baseWidth, ktxTex->baseHeight, 1 },
                                        .usage = lvk::TextureUsageBits_Sampled,
                                        .numMipLevels = ktxTex->numLevels,
                                        .data = ktxTex->pData,
                                        .dataNumMipLevels = ktxTex->numLevels,
                                        .debugName = fileName } };
}

GLTFMaterialDataGPU convertToGPUMaterialLazy(const std::unique_ptr<lvk::IContext>& ctx, const Material& mat, const TextureFiles& files,
                                             TextureCache& cache, std::vector<LoadedTextureData>& loadedTextureData,
                                             std::mutex& loadingMutex)
{
    LVK_PROFILER_FUNCTION();

    GLTFMaterialDataGPU result = {
        .baseColorFactor = mat.baseColorFactor,
        .metallicRoughnessNormalOcclusion = vec4(mat.metallicFactor, mat.roughness, 1.0f, 1.0f),
        .clearcoatTransmissionThickness = vec4(1.0f, 1.0f, mat.transparencyFactor, 1.0f),
        .emissiveFactorAlphaCutoff = vec4(vec3(mat.emissiveFactor), mat.alphaTest),
    };

    auto startLoadingTexture = [&cache, &ctx, &files, &loadedTextureData, &loadingMutex](int textureId)
    {
        if (textureId == -1)
        {
            return;
        }

        if (cache.size() <= textureId)
        {
            cache.resize(textureId + 1);
        }
        // not in the cache and not in the queue
        const bool notInCache = cache[textureId].empty();
        const bool notInQueue =
            std::find_if(loadedTextureData.cbegin(), loadedTextureData.cend(),
                         [textureId](const LoadedTextureData& d) { return d.index == textureId; }) == loadedTextureData.end();
        if (notInCache && notInQueue)
        {
            LoadedTextureData textureData = loadTextureData(files[textureId].c_str());
            if (textureData.ktxTex)
            {
                textureData.index = textureId;
                loadedTextureData.push_back(textureData);
            }
        }
    };

    std::lock_guard lock(loadingMutex);

    startLoadingTexture(mat.baseColorTexture);
    startLoadingTexture(mat.emissiveTexture);
    startLoadingTexture(mat.normalTexture);
    startLoadingTexture(mat.opacityTexture);

    return result;
}

class VKMesh11Lazy final: public VKMesh11
{
public:
    VKMesh11Lazy(const std::unique_ptr<lvk::IContext>& ctx, const MeshData& meshData, const Scene& scene,
                 lvk::StorageType indirectBufferStorage = lvk::StorageType_Device):
        VKMesh11(ctx, meshData, scene, indirectBufferStorage, false)
    {
        materialsGPU_.resize(materialsCPU_.size());

        // construct Taskflow
        taskflow_.for_each_index(0u, static_cast<uint32_t>(materialsCPU_.size()), 1u,
                                 [&](int i) {
                                     materialsGPU_[i] = convertToGPUMaterialLazy(ctx, materialsCPU_[i], textureFiles_, textureCache_,
                                                                                 loadedTextureData_, loadingMutex_);
                                 });

        // start loading
        executor_.run(taskflow_);
    }

    bool processLoadedTextures(lvk::ICommandBuffer& buf, uint32_t maxTexturesPerFrame = 64)
    {
        LVK_PROFILER_FUNCTION();

        std::vector<LoadedTextureData> textures;
        textures.reserve(maxTexturesPerFrame);

        {
            std::unique_lock lock(loadingMutex_, std::try_to_lock);
            if (!lock.owns_lock())
            {
                return false;
            }

            const uint32_t count = std::min<uint32_t>(maxTexturesPerFrame, (uint32_t)loadedTextureData_.size());
            if (!count)
            {
                return false;
            }

            for (uint32_t i = 0; i != count; ++i)
            {
                textures.push_back(loadedTextureData_.back());
                loadedTextureData_.pop_back();
            }
        }

        std::vector<std::pair<uint32_t, lvk::Holder<lvk::TextureHandle>>> uploadedTextures;
        uploadedTextures.reserve(textures.size());
        for (LoadedTextureData& tex : textures)
        {
            uploadedTextures.emplace_back(tex.index, ctx->createTexture(tex.desc));
            ktxTexture_Destroy(ktxTexture(tex.ktxTex));
        }

        // calculate a span of updated materials
        size_t begin = 0;
        size_t end = 0;
        {
            std::lock_guard lock(loadingMutex_);

            for (auto& [textureId, texture] : uploadedTextures)
            {
                textureCache_[textureId] = std::move(texture);
            }

            auto getTextureFromCache = [this](int textureId) -> uint32_t
            { return textureCache_.size() > textureId ? textureCache_[textureId].index() : 0; };

            LVK_ASSERT(materialsCPU_.size() == materialsGPU_.size());

            // go through the texture cache and update materials
            for (size_t i = 0; i != materialsCPU_.size(); i++)
            {
                const Material& mtl = materialsCPU_[i];

                GLTFMaterialDataGPU m = materialsGPU_[i]; // make a local copy

                m.baseColorTexture = getTextureFromCache(mtl.baseColorTexture);
                m.emissiveTexture = getTextureFromCache(mtl.emissiveTexture);
                m.normalTexture = getTextureFromCache(mtl.normalTexture);
                m.transmissionTexture = getTextureFromCache(mtl.opacityTexture);

                if (memcmp(&m, &materialsGPU_[i], sizeof(m)))
                {
                    if (begin == end)
                    {
                        begin = i;
                    }
                    end = i + 1;

                    materialsGPU_[i] = m;
                }
            }
        }

        // update the buffer
        size_t         size = (end - begin) * sizeof(decltype(materialsGPU_)::value_type);
        size_t         offset = begin * sizeof(decltype(materialsGPU_)::value_type);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(materialsGPU_.data());

        while (size)
        {
            const size_t chunk = std::min(size, (size_t)65536);
            buf.cmdUpdateBuffer(bufferMaterials_, offset, chunk, bytes + offset);
            size -= chunk;
            offset += chunk;
        }

        return true;
    }

public:
    // multithreading
    std::mutex                     loadingMutex_;
    std::vector<LoadedTextureData> loadedTextureData_;

    tf::Taskflow taskflow_;
    tf::Executor executor_{ size_t(2) };
};
