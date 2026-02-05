#pragma once

#include <rhi/buffer.h>
#include <rhi/command_list.h>
#include <rhi/enums.h>
#include <rhi/pipeline.h>
#include <rhi/render_target.h>
#include <rhi/rhi.h>
#include <rhi/sampler.h>
#include <rhi/semaphore.h>
#include <rhi/shader.h>
#include <rhi/swap_chain.h>
#include <rhi/texture.h>

#include <core/log.h>
#include <core/definations.h>
#include <core/path.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Horizon::Backend
{

// Forward declarations
class FrameGraph;
class FrameGraphBuilder;

// RDGPass base class - all passes should inherit from this
class RDGPass
{
  public:
    RDGPass(const std::string &name, RHI *rhi = nullptr) : m_name(name), m_rhi(rhi)
    {
    }
    virtual ~RDGPass() = default;

    const std::string &GetName() const
    {
        return m_name;
    }

    // Import resources into FrameGraph (called before Setup)
    virtual void ImportResources(FrameGraph *frame_graph)
    {
    }

    // Setup phase - declare resource usage (called during Compile)
    virtual void Setup(FrameGraphBuilder &builder) = 0;

    // Execute phase - execute actual rendering commands (called during Execute)
    virtual void Execute(CommandList *command_list, FrameGraphBuilder &builder) = 0;

  protected:
    // Helper functions for creating shaders and pipelines
    Shader *CreateShader(ShaderType type, const Path &file_name, const char *entry_point = "main")
    {
        if (m_rhi == nullptr)
        {
            LOG_ERROR("RDGPass::CreateShader: RHI is null. Pass must be constructed with RHI pointer.");
            return nullptr;
        }
        return m_rhi->CreateShader(type, file_name, entry_point);
    }

    Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info)
    {
        if (m_rhi == nullptr)
        {
            LOG_ERROR("RDGPass::CreateGraphicsPipeline: RHI is null. Pass must be constructed with RHI pointer.");
            return nullptr;
        }
        return m_rhi->CreateGraphicsPipeline(create_info);
    }

    Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info = {})
    {
        if (m_rhi == nullptr)
        {
            LOG_ERROR("RDGPass::CreateComputePipeline: RHI is null. Pass must be constructed with RHI pointer.");
            return nullptr;
        }
        return m_rhi->CreateComputePipeline(create_info);
    }

    void DestroyShader(Shader *shader)
    {
        if (m_rhi != nullptr && shader != nullptr)
        {
            m_rhi->DestroyShader(shader);
        }
    }

    void DestroyPipeline(Pipeline *pipeline)
    {
        if (m_rhi != nullptr && pipeline != nullptr)
        {
            m_rhi->DestroyPipeline(pipeline);
        }
    }

    RHI *GetRHI() const
    {
        return m_rhi;
    }

    std::string m_name;
    RHI *m_rhi;
};

// Resource handle types
struct TextureHandle
{
    u32 index;
    bool IsValid() const
    {
        return index != UINT32_MAX;
    }
    static TextureHandle Invalid()
    {
        return {UINT32_MAX};
    }
};

struct BufferHandle
{
    u32 index;
    bool IsValid() const
    {
        return index != UINT32_MAX;
    }
    static BufferHandle Invalid()
    {
        return {UINT32_MAX};
    }
};

struct RenderTargetHandle
{
    u32 index;
    bool IsValid() const
    {
        return index != UINT32_MAX;
    }
    static RenderTargetHandle Invalid()
    {
        return {UINT32_MAX};
    }
};

// Resource usage information
struct ResourceUsage
{
    ResourceState state = ResourceState::RESOURCE_STATE_UNDEFINED;
    u32 first_mip = 0;
    u32 mip_count = 1;
    u32 first_layer = 0;
    u32 layer_count = 1;
    bool is_read = false;
    bool is_write = false;
};

// Pass setup callback - declares resource usage (called during Compile)
using PassSetupCallback = std::function<void(FrameGraphBuilder &builder)>;

// Pass execution callback - executes actual rendering commands (called during Execute)
using PassExecuteCallback = std::function<void(CommandList *command_list, FrameGraphBuilder &builder)>;

// Pass node in the graph
struct PassNode
{
    std::string name;
    PassSetupCallback setup_callback;
    PassExecuteCallback execute_callback;
    RDGPass *rdg_pass = nullptr; // Optional: pointer to RDGPass instance

    // Resource dependencies
    std::vector<TextureHandle> read_textures;
    std::vector<TextureHandle> write_textures;
    std::vector<BufferHandle> read_buffers;
    std::vector<BufferHandle> write_buffers;
    std::vector<RenderTargetHandle> render_targets;

    // Resource usage information
    std::unordered_map<u32, ResourceUsage> texture_usages;
    std::unordered_map<u32, ResourceUsage> buffer_usages;
};

// Resource node
struct TextureResource
{
    std::string name;
    TextureCreateInfo create_info;
    Texture *actual_texture = nullptr; // Actual RHI resource (for imported resources)
    bool is_imported = false;
    bool is_transient = true; // Transient resources are created/destroyed each frame
    bool is_managed = false;  // Managed by FrameGraph (created/destroyed by FrameGraph)
};

struct BufferResource
{
    std::string name;
    BufferCreateInfo create_info;
    Buffer *actual_buffer = nullptr;
    bool is_imported = false;
    bool is_transient = true;
    bool is_managed = false; // Managed by FrameGraph (created/destroyed by FrameGraph)
};

struct RenderTargetResource
{
    std::string name;
    RenderTargetCreateInfo create_info;
    RenderTarget *actual_render_target = nullptr;
    bool is_imported = false;
    bool is_transient = true;
    bool is_managed = false; // Managed by FrameGraph (created/destroyed by FrameGraph)
};

// FrameGraphBuilder - used during pass setup
class FrameGraphBuilder
{
  public:
    FrameGraphBuilder(FrameGraph *graph, PassNode *pass) : m_graph(graph), m_pass(pass)
    {
    }

    // Declare/create resources
    TextureHandle CreateTexture(const std::string &name, const TextureCreateInfo &create_info);
    TextureHandle ImportTexture(const std::string &name, Texture *texture);
    BufferHandle CreateBuffer(const std::string &name, const BufferCreateInfo &create_info);
    BufferHandle ImportBuffer(const std::string &name, Buffer *buffer);
    RenderTargetHandle CreateRenderTarget(const std::string &name, const RenderTargetCreateInfo &create_info);
    RenderTargetHandle ImportRenderTarget(const std::string &name, RenderTarget *render_target);

    // Read/Write resource declarations
    void ReadTexture(TextureHandle handle, ResourceState state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    void WriteTexture(TextureHandle handle, ResourceState state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    void ReadBuffer(BufferHandle handle, ResourceState state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    void WriteBuffer(BufferHandle handle, ResourceState state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);

    // Render target operations
    void UseRenderTarget(RenderTargetHandle handle);

    // Get actual resources (for setting up pipelines, etc.)
    Texture *GetTexture(TextureHandle handle) const;
    Buffer *GetBuffer(BufferHandle handle) const;
    RenderTarget *GetRenderTarget(RenderTargetHandle handle) const;

  private:
    FrameGraph *m_graph;
    PassNode *m_pass;
};

// FrameGraph - main render graph class
class FrameGraph
{
  public:
    FrameGraph(RHI *rhi);
    ~FrameGraph();

    // Setup phase - declare passes and resources
    // setup_callback: declares resource usage (ReadTexture/WriteTexture/etc.)
    // execute_callback: executes actual rendering commands
    FrameGraphBuilder AddPass(const std::string &name, PassSetupCallback setup_callback,
                              PassExecuteCallback execute_callback);

    // Add pass using RDGPass base class
    FrameGraphBuilder AddPass(RDGPass *pass);

    // Compile phase - analyze dependencies and create barriers
    void Compile();

    // Execute phase - execute all passes with automatic barriers
    void Execute();

    // Reset for next frame
    void Reset();

    // Get actual resources (for external use)
    Texture *GetTexture(TextureHandle handle) const;
    Buffer *GetBuffer(BufferHandle handle) const;
    RenderTarget *GetRenderTarget(RenderTargetHandle handle) const;

    // Import external resources
    TextureHandle ImportTexture(const std::string &name, Texture *texture);
    BufferHandle ImportBuffer(const std::string &name, Buffer *buffer);
    RenderTargetHandle ImportRenderTarget(const std::string &name, RenderTarget *render_target);

  private:
    friend class FrameGraphBuilder;

    RHI *m_rhi;

    // Resources
    std::vector<TextureResource> m_textures;
    std::vector<BufferResource> m_buffers;
    std::vector<RenderTargetResource> m_render_targets;

    // Passes
    std::vector<PassNode> m_passes;

    // Resource name to handle mapping
    std::unordered_map<std::string, TextureHandle> m_texture_name_map;
    std::unordered_map<std::string, BufferHandle> m_buffer_name_map;
    std::unordered_map<std::string, RenderTargetHandle> m_render_target_name_map;

    // Compiled execution order
    std::vector<u32> m_execution_order;

    // Helper functions
    TextureHandle FindOrCreateTexture(const std::string &name);
    BufferHandle FindOrCreateBuffer(const std::string &name);
    RenderTargetHandle FindOrCreateRenderTarget(const std::string &name);

    void InsertBarriers(CommandList *command_list, u32 pass_index);
    ResourceState GetLastState(TextureHandle handle, u32 pass_index);
    ResourceState GetLastState(BufferHandle handle, u32 pass_index);
    bool WasTextureWrittenByPreviousPass(TextureHandle handle, u32 pass_index);
    bool WasBufferWrittenByPreviousPass(BufferHandle handle, u32 pass_index);
};

} // namespace Horizon::Backend
