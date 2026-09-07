#pragma once

#include "MeshUtils.h"

#include <cstring>
#include <utility>

class VKIndirectBuffer11 final
{
public:
    VKIndirectBuffer11(const std::unique_ptr<lvk::IContext>& ctx, size_t maxDrawCommands,
                       lvk::StorageType indirectBufferStorage = lvk::StorageType_Device):
        ctx_(ctx),
        drawCommands_(maxDrawCommands)
    {
        // Indirect buffer layout: | uint32_t: numCommands | DrawIndexedIndirectCommand | DrawIndexedIndirectCommand |
        // ... indirect draw buffer, constructed by
        bufferIndirect_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Indirect | lvk::BufferUsageBits_Storage,
                                              .storage = indirectBufferStorage,
                                              .size = sizeof(DrawIndexedIndirectCommand) * maxDrawCommands + sizeof(uint32_t),
                                              .debugName = "Buffer: indirect" },
                                            nullptr);
        bufferDrawData_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Storage,
                                              .storage = indirectBufferStorage,
                                              .size = sizeof(DrawData) * maxDrawCommands,
                                              .debugName = "Buffer: indirect drawData" },
                                            nullptr);
    }

    void uploadIndirectBuffer()
    {
        const uint32_t numCommands = drawCommands_.size();
        // store the number of draw commands in the very beginning of the indirect buffer
        ctx_->upload(bufferIndirect_, &numCommands, sizeof(uint32_t));
        ctx_->upload(bufferIndirect_, drawCommands_.data(), sizeof(VkDrawIndexedIndirectCommand) * numCommands, sizeof(uint32_t));
        if (!drawData_.empty())
        {
            ctx_->upload(bufferDrawData_, drawData_.data(), sizeof(DrawData) * drawData_.size());
        }
    };

    void selectTo(VKIndirectBuffer11& buf, const std::vector<DrawData>& sourceDrawData,
                  const std::function<bool(const DrawIndexedIndirectCommand&)>& pred) const
    {
        buf.drawCommands_.clear();
        buf.drawData_.clear();
        for (const auto& c : drawCommands_)
        {
            if (pred(c))
            {
                DrawIndexedIndirectCommand selected = c;
                selected.baseInstance = static_cast<uint32_t>(buf.drawData_.size());
                buf.drawCommands_.push_back(selected);
                buf.drawData_.push_back(sourceDrawData[c.baseInstance]);
            }
        }
        buf.uploadIndirectBuffer();
    }

    DrawIndexedIndirectCommand* getDrawIndexedIndirectCommandPtr() const
    {
        LVK_ASSERT(ctx_->getMappedPtr(bufferIndirect_));
        return (DrawIndexedIndirectCommand*)(ctx_->getMappedPtr(bufferIndirect_) + sizeof(uint32_t));
    }

    void flushActiveCommands(uint32_t numCommands) const
    {
        ctx_->flushMappedMemory(bufferIndirect_, 0, sizeof(uint32_t) + sizeof(DrawIndexedIndirectCommand) * numCommands);
        ctx_->flushMappedMemory(bufferDrawData_, 0, sizeof(DrawData) * numCommands);
    }

    void restoreAllCommands() const
    {
        LVK_ASSERT(ctx_->getMappedPtr(bufferIndirect_));
        LVK_ASSERT(ctx_->getMappedPtr(bufferDrawData_));
        uint32_t*                   drawCount = (uint32_t*)ctx_->getMappedPtr(bufferIndirect_);
        DrawIndexedIndirectCommand* dst = getDrawIndexedIndirectCommandPtr();
        DrawData*                   dstDrawData = (DrawData*)ctx_->getMappedPtr(bufferDrawData_);
        const uint32_t              numCommands = static_cast<uint32_t>(drawCommands_.size());
        *drawCount = numCommands;
        if (numCommands)
        {
            std::memcpy(dst, drawCommands_.data(), sizeof(DrawIndexedIndirectCommand) * numCommands);
            std::memcpy(dstDrawData, drawData_.data(), sizeof(DrawData) * numCommands);
        }
        flushActiveCommands(numCommands);
    }

public:
    const std::unique_ptr<lvk::IContext>& ctx_;

    lvk::Holder<lvk::BufferHandle> bufferIndirect_;
    lvk::Holder<lvk::BufferHandle> bufferDrawData_;

    std::vector<DrawIndexedIndirectCommand> drawCommands_;
    std::vector<DrawData>                   drawData_;
};

class VKPipeline
{
public:
    VKPipeline() = default;

    VKPipeline(const std::unique_ptr<lvk::IContext>& ctx, const lvk::VertexInput& streams, lvk::Format colorFormat, lvk::Format depthFormat,
               uint32_t numSamples = 1, lvk ::Holder<lvk::ShaderModuleHandle>&& vert = {}, lvk::Holder<lvk::ShaderModuleHandle>&& frag = {},
               bool positionOnly = false):
        positionOnly_(positionOnly)
    {
        vert_ = vert.valid() ? std::move(vert) : loadShaderModule(ctx, "PTRenderer/shaders/main.vert");
        LVK_ASSERT(frag.valid());
        frag_ = std::move(frag);

        pipeline_ = ctx->createRenderPipeline({
            .vertexInput = streams,
            .smVert = vert_,
            .smFrag = frag_,
            .color = { { .format = colorFormat } },
            .depthFormat = depthFormat,
            .cullMode = lvk::CullMode_Back,
            .samplesCount = numSamples,
            .minSampleShading = 0.0f,
        });

        LVK_ASSERT(pipeline_.valid());
    }

public:
    lvk::Holder<lvk::ShaderModuleHandle> vert_;
    lvk::Holder<lvk::ShaderModuleHandle> frag_;

    lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
    bool                                   positionOnly_ = false;
};

class VkPipelineDeferred: public VKPipeline
{
public:
    VkPipelineDeferred(const std::unique_ptr<lvk::IContext>& ctx, const lvk::VertexInput& streams, lvk::Format colorFormats,
                       lvk::Format depthFormat, uint32_t numSamples = 1, lvk::Holder<lvk::ShaderModuleHandle>&& vert = {},
                       lvk::Holder<lvk::ShaderModuleHandle>&& frag = {}, bool positionOnly = false)
    {
        positionOnly_ = positionOnly;
        vert_ = vert.valid() ? std::move(vert) : loadShaderModule(ctx, "PTRenderer/src/gbuffer_opaque.vert");
        frag_ = frag.valid() ? std::move(frag) : loadShaderModule(ctx, "PTRenderer/src/gbuffer_opaque.frag");
        lvk::ColorAttachment attachments[8] = {};
        attachments[0].format = lvk::Format::Format_R11G11B10_F;
        attachments[1].format = lvk::Format::Format_A2B10G10R10_UN;
        attachments[2].format = lvk::Format::Format_RGBA_UN8;
        attachments[3].format = lvk::Format::Format_RGBA_UN8;
        pipeline_ = ctx->createRenderPipeline({
            .vertexInput = streams,
            .smVert = vert_,
            .smFrag = frag_,
            .color = { attachments[0], attachments[1], attachments[2], attachments[3] },
            .depthFormat = depthFormat,
            .cullMode = lvk::CullMode_Back,
            .samplesCount = numSamples,
            .minSampleShading = 0.0f,
        });
        LVK_ASSERT(pipeline_.valid());
    }
};

class VKMesh11
{
public:
    VKMesh11(const std::unique_ptr<lvk::IContext>& ctx, const MeshData& meshData, const Scene& scene,
             lvk::StorageType indirectBufferStorage = lvk::StorageType_Device, bool preloadMaterials = true):
        ctx(ctx),
        numIndices_((uint32_t)meshData.indexData.size()), numMeshes_((uint32_t)meshData.meshes.size()),
        indirectBuffer_(ctx, meshData.getMeshFileHeader().meshCount, indirectBufferStorage), textureFiles_(meshData.textureFiles)
    {
        const MeshFileHeader header = meshData.getMeshFileHeader();

        const uint32_t* indices = meshData.indexData.data();
        const uint8_t*  positionData = meshData.positionData.data();
        const uint8_t*  attributeData = meshData.attributeData.data();

        materialsCPU_ = meshData.materials;
        materialsGPU_.reserve(meshData.materials.size());

        for (const auto& mat : meshData.materials)
        {
            materialsGPU_.push_back(preloadMaterials ? convertToGPUMaterial(ctx, mat, textureFiles_, textureCache_)
                                                     : GLTFMaterialDataGPU{});
        }

        bufferPositions_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_AccelStructBuildInputReadOnly,
                                               .storage = lvk::StorageType_Device,
                                               .size = header.positionDataSize,
                                               .data = positionData,
                                               .debugName = "Buffer: vertex positions" },
                                             nullptr);
        bufferAttributes_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
                                                .storage = lvk::StorageType_Device,
                                                .size = header.attributeDataSize,
                                                .data = attributeData,
                                                .debugName = "Buffer: vertex attributes" },
                                              nullptr);
        bufferIndices_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Index | lvk::BufferUsageBits_AccelStructBuildInputReadOnly,
                                             .storage = lvk::StorageType_Device,
                                             .size = header.indexDataSize,
                                             .data = indices,
                                             .debugName = "Buffer: index" },
                                           nullptr);
        bufferTransforms_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Storage,
                                                .storage = lvk::StorageType_Device,
                                                .size = scene.globalTransform.size() * sizeof(glm::mat4),
                                                .data = scene.globalTransform.data(),
                                                .debugName = "Buffer: transforms" },
                                              nullptr);
        bufferMaterials_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Storage,
                                               .storage = lvk::StorageType_Device,
                                               .size = meshData.materials.size() * sizeof(decltype(materialsGPU_)::value_type),
                                               .data = materialsGPU_.data(),
                                               .debugName = "Buffer: materials" },
                                             nullptr);

        const uint32_t numCommands = header.meshCount;

        indirectBuffer_.drawCommands_.resize(numCommands);
        drawData_.resize(numCommands);

        DrawIndexedIndirectCommand* cmd = indirectBuffer_.drawCommands_.data();
        DrawData*                   dd = drawData_.data();

        LVK_ASSERT(scene.meshForNode.size() == numCommands);

        uint32_t ddIndex = 0;

        // prepare indirect commands buffer
        for (auto& i : scene.meshForNode)
        {
            const Mesh& mesh = meshData.meshes[i.second];

            const uint32_t lod = std::min(0u, mesh.lodCount - 1); // TODO: implement dynamic lod

            *cmd++ = {
                .count = mesh.getLODIndicesCount(lod),
                .instanceCount = 1,
                .firstIndex = mesh.indexOffset, // + mesh.lodOffset[lod],
                .baseVertex = (int32_t)mesh.vertexOffset,
                .baseInstance = ddIndex++,
            };
            *dd++ = {
                .transformId = i.first,
                .materialId = mesh.materialID,
            };
        }
        indirectBuffer_.drawData_ = drawData_;
        indirectBuffer_.uploadIndirectBuffer();

        bufferDrawData_ = ctx->createBuffer({ .usage = lvk::BufferUsageBits_Storage,
                                              .storage = lvk::StorageType_Device,
                                              .size = sizeof(DrawData) * numCommands,
                                              .data = drawData_.data(),
                                              .debugName = "Buffer: drawData" },
                                            nullptr);

        rayTracing_ = FinalDemo::createRayTracingScene(ctx, meshData, scene, bufferPositions_, bufferIndices_);
    }

    void draw(lvk::ICommandBuffer& buf, const VKPipeline& pipeline, const mat4& view, const mat4& proj,
              lvk::TextureHandle texSkyboxIrradiance = {}, const VKIndirectBuffer11* indirectBuffer = nullptr) const
    {
        if (!indirectBuffer)
        {
            indirectBuffer = &indirectBuffer_;
        }
        buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
        bindVertexBuffers(buf, pipeline);
        buf.cmdBindRenderPipeline(pipeline.pipeline_);
        buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
        const struct
        {
            mat4     viewProj;
            uint64_t bufferTransforms;
            uint64_t bufferDrawData;
            uint64_t bufferMaterials;
            uint32_t texSkyboxIrradiance;
        } pc = {
            .viewProj = proj * view,
            .bufferTransforms = ctx->gpuAddress(bufferTransforms_),
            .bufferDrawData = ctx->gpuAddress(bufferDrawData_),
            .bufferMaterials = ctx->gpuAddress(bufferMaterials_),
            .texSkyboxIrradiance = texSkyboxIrradiance.index(),
        };
        static_assert(sizeof(pc) <= 128);
        buf.cmdPushConstants(pc);
        buf.cmdDrawIndexedIndirectCount(indirectBuffer->bufferIndirect_, sizeof(uint32_t), indirectBuffer->bufferIndirect_, 0, numMeshes_,
                                        sizeof(DrawIndexedIndirectCommand));
    }

    void draw(lvk::ICommandBuffer& buf, const VKPipeline& pipeline, const void* pushConstants, size_t pcSize,
              const lvk::DepthState     depthState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true },
              const VKIndirectBuffer11* indirectBuffer = nullptr) const
    {
        buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
        bindVertexBuffers(buf, pipeline);
        buf.cmdBindRenderPipeline(pipeline.pipeline_);
        buf.cmdBindDepthState(depthState);
        buf.cmdPushConstants(pushConstants, pcSize);
        if (!indirectBuffer)
        {
            indirectBuffer = &indirectBuffer_;
        }
        buf.cmdDrawIndexedIndirectCount(indirectBuffer->bufferIndirect_, sizeof(uint32_t), indirectBuffer->bufferIndirect_, 0, numMeshes_,
                                        sizeof(DrawIndexedIndirectCommand));
    }

    DrawIndexedIndirectCommand* getDrawIndexedIndirectCommandPtr() const { return indirectBuffer_.getDrawIndexedIndirectCommandPtr(); };

    void bindVertexBuffers(lvk::ICommandBuffer& buf, const VKPipeline& pipeline) const
    {
        buf.cmdBindVertexBuffer(0, bufferPositions_);
        if (!pipeline.positionOnly_)
        {
            buf.cmdBindVertexBuffer(1, bufferAttributes_);
        }
    }

public:
    const std::unique_ptr<lvk::IContext>& ctx;

    uint32_t numIndices_ = 0;
    uint32_t numMeshes_ = 0;

    lvk::Holder<lvk::BufferHandle> bufferIndices_;
    lvk::Holder<lvk::BufferHandle> bufferPositions_;
    lvk::Holder<lvk::BufferHandle> bufferAttributes_;
    lvk::Holder<lvk::BufferHandle> bufferTransforms_;
    lvk::Holder<lvk::BufferHandle> bufferDrawData_;
    lvk::Holder<lvk::BufferHandle> bufferMaterials_;
    FinalDemo::RayTracingScene     rayTracing_;

    std::vector<DrawData> drawData_;

    VKIndirectBuffer11 indirectBuffer_;

    TextureFiles         textureFiles_;
    mutable TextureCache textureCache_;

    std::vector<Material>            materialsCPU_;
    std::vector<GLTFMaterialDataGPU> materialsGPU_;
};
