#include "frame_graph.h"

#include <algorithm>
#include <core/definations.h>
#include <core/log.h>
#include <rhi/resource_barrier.h>
#include <unordered_set>

namespace Horizon::Backend
{

// FrameGraphBuilder implementation
TextureHandle FrameGraphBuilder::CreateTexture(const std::string &name, const TextureCreateInfo &create_info)
{
    auto handle = m_graph->FindOrCreateTexture(name);
    if (handle.IsValid())
    {
        m_graph->m_textures[handle.index].create_info = create_info;
        m_graph->m_textures[handle.index].is_managed = true;  // Mark as managed by FrameGraph
        m_graph->m_textures[handle.index].is_transient = false; // Managed resources persist
    }
    return handle;
}

TextureHandle FrameGraphBuilder::ImportTexture(const std::string &name, Texture *texture)
{
    return m_graph->ImportTexture(name, texture);
}

BufferHandle FrameGraphBuilder::CreateBuffer(const std::string &name, const BufferCreateInfo &create_info)
{
    auto handle = m_graph->FindOrCreateBuffer(name);
    if (handle.IsValid())
    {
        m_graph->m_buffers[handle.index].create_info = create_info;
        m_graph->m_buffers[handle.index].is_managed = true;  // Mark as managed by FrameGraph
        m_graph->m_buffers[handle.index].is_transient = false; // Managed resources persist
    }
    return handle;
}

BufferHandle FrameGraphBuilder::ImportBuffer(const std::string &name, Buffer *buffer)
{
    return m_graph->ImportBuffer(name, buffer);
}

RenderTargetHandle FrameGraphBuilder::CreateRenderTarget(const std::string &name,
                                                         const RenderTargetCreateInfo &create_info)
{
    auto handle = m_graph->FindOrCreateRenderTarget(name);
    if (handle.IsValid())
    {
        m_graph->m_render_targets[handle.index].create_info = create_info;
        m_graph->m_render_targets[handle.index].is_managed = true;  // Mark as managed by FrameGraph
        m_graph->m_render_targets[handle.index].is_transient = false; // Managed resources persist
    }
    return handle;
}

RenderTargetHandle FrameGraphBuilder::ImportRenderTarget(const std::string &name, RenderTarget *render_target)
{
    return m_graph->ImportRenderTarget(name, render_target);
}

void FrameGraphBuilder::ReadTexture(TextureHandle handle, ResourceState state)
{
    if (!handle.IsValid())
        return;

    m_pass->read_textures.push_back(handle);
    ResourceUsage usage;
    usage.state = state;
    usage.is_read = true;
    m_pass->texture_usages[handle.index] = usage;
}

void FrameGraphBuilder::WriteTexture(TextureHandle handle, ResourceState state)
{
    if (!handle.IsValid())
        return;

    m_pass->write_textures.push_back(handle);
    ResourceUsage usage;
    usage.state = state;
    usage.is_write = true;
    m_pass->texture_usages[handle.index] = usage;
}

void FrameGraphBuilder::ReadBuffer(BufferHandle handle, ResourceState state)
{
    if (!handle.IsValid())
        return;

    m_pass->read_buffers.push_back(handle);
    ResourceUsage usage;
    usage.state = state;
    usage.is_read = true;
    m_pass->buffer_usages[handle.index] = usage;
}

void FrameGraphBuilder::WriteBuffer(BufferHandle handle, ResourceState state)
{
    if (!handle.IsValid())
        return;

    m_pass->write_buffers.push_back(handle);
    ResourceUsage usage;
    usage.state = state;
    usage.is_write = true;
    m_pass->buffer_usages[handle.index] = usage;
}

void FrameGraphBuilder::UseRenderTarget(RenderTargetHandle handle)
{
    if (!handle.IsValid())
        return;

    m_pass->render_targets.push_back(handle);
}

Texture *FrameGraphBuilder::GetTexture(TextureHandle handle) const
{
    return m_graph->GetTexture(handle);
}

Buffer *FrameGraphBuilder::GetBuffer(BufferHandle handle) const
{
    return m_graph->GetBuffer(handle);
}

RenderTarget *FrameGraphBuilder::GetRenderTarget(RenderTargetHandle handle) const
{
    return m_graph->GetRenderTarget(handle);
}

// FrameGraph implementation
FrameGraph::FrameGraph(RHI *rhi) : m_rhi(rhi)
{
}

FrameGraph::~FrameGraph()
{
    Reset();
}

FrameGraphBuilder FrameGraph::AddPass(const std::string &name, PassSetupCallback setup_callback,
                                      PassExecuteCallback execute_callback)
{
    PassNode pass;
    pass.name = name;
    pass.setup_callback = setup_callback;
    pass.execute_callback = execute_callback;

    m_passes.push_back(std::move(pass));
    return FrameGraphBuilder(this, &m_passes.back());
}

FrameGraphBuilder FrameGraph::AddPass(RDGPass *pass)
{
    if (pass == nullptr)
    {
        LOG_ERROR("Cannot add null RDGPass");
        return FrameGraphBuilder(this, nullptr);
    }

    // Call ImportResources before adding pass
    pass->ImportResources(this);

    PassNode pass_node;
    pass_node.name = pass->GetName();
    pass_node.rdg_pass = pass;
    // Create callbacks that delegate to RDGPass methods
    pass_node.setup_callback = [pass](FrameGraphBuilder &builder) { pass->Setup(builder); };
    pass_node.execute_callback = [pass](CommandList *cl, FrameGraphBuilder &builder) { pass->Execute(cl, builder); };

    m_passes.push_back(std::move(pass_node));
    return FrameGraphBuilder(this, &m_passes.back());
}

void FrameGraph::Compile()
{
    // Simple execution order: just use the order passes were added
    // TODO: Implement topological sort for better optimization
    m_execution_order.clear();
    for (u32 i = 0; i < m_passes.size(); ++i)
    {
        m_execution_order.push_back(i);
    }

    // Create transient and managed resources
    for (auto &tex : m_textures)
    {
        if (tex.actual_texture == nullptr)
        {
            if ((tex.is_transient || tex.is_managed) && !tex.is_imported)
            {
                tex.actual_texture = m_rhi->CreateTexture(tex.create_info);
            }
        }
    }

    for (auto &buf : m_buffers)
    {
        if (buf.actual_buffer == nullptr)
        {
            if ((buf.is_transient || buf.is_managed) && !buf.is_imported)
            {
                buf.actual_buffer = m_rhi->CreateBuffer(buf.create_info);
            }
        }
    }

    for (auto &rt : m_render_targets)
    {
        if (rt.actual_render_target == nullptr)
        {
            if ((rt.is_transient || rt.is_managed) && !rt.is_imported)
            {
                rt.actual_render_target = m_rhi->CreateRenderTarget(rt.create_info);
            }
        }
    }

    // Setup phase: Execute all pass setup callbacks to collect resource usage information
    for (u32 pass_idx : m_execution_order)
    {
        auto &pass = m_passes[pass_idx];
        FrameGraphBuilder builder(this, &pass);
        // Call setup callback to declare resource usage
        if (pass.rdg_pass)
        {
            pass.rdg_pass->Setup(builder);
        }
        else if (pass.setup_callback)
        {
            pass.setup_callback(builder);
        }
    }
}

void FrameGraph::Execute()
{
    CommandList *current_command_list = nullptr;
    CommandQueueType current_queue = CommandQueueType::GRAPHICS;

    // Track command lists by queue type for submission
    std::vector<CommandList *> command_lists;

    for (u32 pass_idx : m_execution_order)
    {
        auto &pass = m_passes[pass_idx];

        // Get command list for this queue
        if (current_command_list == nullptr)
        {
            if (current_command_list != nullptr)
            {
                current_command_list->EndRecording();
                command_lists.push_back(current_command_list);
            }
            current_command_list = m_rhi->GetCommandList(current_queue);
            current_command_list->BeginRecording();
        }

        // Insert barriers before pass
        InsertBarriers(current_command_list, pass_idx);

        // Execute pass
        FrameGraphBuilder builder(this, &pass);
        if (pass.rdg_pass)
        {
            pass.rdg_pass->Execute(current_command_list, builder);
        }
        else if (pass.execute_callback)
        {
            // LOG_DEBUG("pass execute: {}", pass.name);
            pass.execute_callback(current_command_list, builder);
        }
    }

    // End the last command list
    if (current_command_list != nullptr)
    {
        current_command_list->EndRecording();
        command_lists.push_back(current_command_list);
    }

    // Submit all command lists
    if (!command_lists.empty())
    {
        QueueSubmitInfo submit_info{};
        submit_info.queue_type = CommandQueueType::GRAPHICS;
        submit_info.command_lists = command_lists;
        submit_info.wait_image_acquired = true;
        submit_info.signal_render_complete = true;
        m_rhi->SubmitCommandLists(submit_info);
    }
}

void FrameGraph::Reset()
{
    // Destroy transient resources (managed resources are kept alive)
    for (auto &tex : m_textures)
    {
        if (tex.is_transient && !tex.is_imported && tex.actual_texture != nullptr)
        {
            m_rhi->DestroyTexture(tex.actual_texture);
            tex.actual_texture = nullptr;
        }
        // Note: managed resources are not destroyed here, they persist across frames
    }

    for (auto &buf : m_buffers)
    {
        if (buf.is_transient && !buf.is_imported && buf.actual_buffer != nullptr)
        {
            m_rhi->DestroyBuffer(buf.actual_buffer);
            buf.actual_buffer = nullptr;
        }
        // Note: managed resources are not destroyed here, they persist across frames
    }

    for (auto &rt : m_render_targets)
    {
        if (rt.is_transient && !rt.is_imported && rt.actual_render_target != nullptr)
        {
            m_rhi->DestroyRenderTarget(rt.actual_render_target);
            rt.actual_render_target = nullptr;
        }
        // Note: managed resources are not destroyed here, they persist across frames
    }

    // Clear passes and transient resources (keep managed resources)
    m_passes.clear();
    // Only clear transient resources, keep managed and imported ones
    auto it_tex = std::remove_if(m_textures.begin(), m_textures.end(),
                                 [](const TextureResource &r) { return r.is_transient && !r.is_imported; });
    m_textures.erase(it_tex, m_textures.end());

    auto it_buf = std::remove_if(m_buffers.begin(), m_buffers.end(),
                                 [](const BufferResource &r) { return r.is_transient && !r.is_imported; });
    m_buffers.erase(it_buf, m_buffers.end());

    auto it_rt = std::remove_if(m_render_targets.begin(), m_render_targets.end(),
                                [](const RenderTargetResource &r) { return r.is_transient && !r.is_imported; });
    m_render_targets.erase(it_rt, m_render_targets.end());

    // Rebuild name maps (only for remaining resources)
    m_texture_name_map.clear();
    for (u32 i = 0; i < m_textures.size(); ++i)
    {
        m_texture_name_map[m_textures[i].name] = TextureHandle{i};
    }

    m_buffer_name_map.clear();
    for (u32 i = 0; i < m_buffers.size(); ++i)
    {
        m_buffer_name_map[m_buffers[i].name] = BufferHandle{i};
    }

    m_render_target_name_map.clear();
    for (u32 i = 0; i < m_render_targets.size(); ++i)
    {
        m_render_target_name_map[m_render_targets[i].name] = RenderTargetHandle{i};
    }

    m_execution_order.clear();
}

Texture *FrameGraph::GetTexture(TextureHandle handle) const
{
    if (!handle.IsValid() || handle.index >= m_textures.size())
        return nullptr;
    return m_textures[handle.index].actual_texture;
}

Buffer *FrameGraph::GetBuffer(BufferHandle handle) const
{
    if (!handle.IsValid() || handle.index >= m_buffers.size())
        return nullptr;
    return m_buffers[handle.index].actual_buffer;
}

RenderTarget *FrameGraph::GetRenderTarget(RenderTargetHandle handle) const
{
    if (!handle.IsValid() || handle.index >= m_render_targets.size())
        return nullptr;
    return m_render_targets[handle.index].actual_render_target;
}

TextureHandle FrameGraph::ImportTexture(const std::string &name, Texture *texture)
{
    auto it = m_texture_name_map.find(name);
    if (it != m_texture_name_map.end())
    {
        return it->second;
    }

    TextureResource resource;
    resource.name = name;
    resource.actual_texture = texture;
    resource.is_imported = true;
    resource.is_transient = false;

    u32 index = static_cast<u32>(m_textures.size());
    m_textures.push_back(resource);
    m_texture_name_map[name] = TextureHandle{index};

    return TextureHandle{index};
}

BufferHandle FrameGraph::ImportBuffer(const std::string &name, Buffer *buffer)
{
    auto it = m_buffer_name_map.find(name);
    if (it != m_buffer_name_map.end())
    {
        return it->second;
    }

    BufferResource resource;
    resource.name = name;
    resource.actual_buffer = buffer;
    resource.is_imported = true;
    resource.is_transient = false;

    u32 index = static_cast<u32>(m_buffers.size());
    m_buffers.push_back(resource);
    m_buffer_name_map[name] = BufferHandle{index};

    return BufferHandle{index};
}

RenderTargetHandle FrameGraph::ImportRenderTarget(const std::string &name, RenderTarget *render_target)
{
    auto it = m_render_target_name_map.find(name);
    if (it != m_render_target_name_map.end())
    {
        return it->second;
    }

    RenderTargetResource resource;
    resource.name = name;
    resource.actual_render_target = render_target;
    resource.is_imported = true;
    resource.is_transient = false;

    u32 index = static_cast<u32>(m_render_targets.size());
    m_render_targets.push_back(resource);
    m_render_target_name_map[name] = RenderTargetHandle{index};

    return RenderTargetHandle{index};
}

TextureHandle FrameGraph::FindOrCreateTexture(const std::string &name)
{
    auto it = m_texture_name_map.find(name);
    if (it != m_texture_name_map.end())
    {
        return it->second;
    }

    TextureResource resource;
    resource.name = name;
    resource.is_transient = true;

    u32 index = static_cast<u32>(m_textures.size());
    m_textures.push_back(resource);
    m_texture_name_map[name] = TextureHandle{index};

    return TextureHandle{index};
}

BufferHandle FrameGraph::FindOrCreateBuffer(const std::string &name)
{
    auto it = m_buffer_name_map.find(name);
    if (it != m_buffer_name_map.end())
    {
        return it->second;
    }

    BufferResource resource;
    resource.name = name;
    resource.is_transient = true;

    u32 index = static_cast<u32>(m_buffers.size());
    m_buffers.push_back(resource);
    m_buffer_name_map[name] = BufferHandle{index};

    return BufferHandle{index};
}

RenderTargetHandle FrameGraph::FindOrCreateRenderTarget(const std::string &name)
{
    auto it = m_render_target_name_map.find(name);
    if (it != m_render_target_name_map.end())
    {
        return it->second;
    }

    RenderTargetResource resource;
    resource.name = name;
    resource.is_transient = true;

    u32 index = static_cast<u32>(m_render_targets.size());
    m_render_targets.push_back(resource);
    m_render_target_name_map[name] = RenderTargetHandle{index};

    return RenderTargetHandle{index};
}

void FrameGraph::InsertBarriers(CommandList *command_list, u32 pass_index)
{
    auto &pass = m_passes[pass_index];
    BarrierDesc barrier;

    // Track which textures/buffers we've already processed to avoid duplicates
    std::unordered_set<u32> processed_textures;
    std::unordered_set<u32> processed_buffers;

    // Process texture barriers - prioritize writes over reads for same resource
    // (writes change state, so if a resource is both read and written, use write state)
    for (auto handle : pass.write_textures)
    {
        if (processed_textures.find(handle.index) != processed_textures.end())
            continue;

        ResourceState last_state = GetLastState(handle, pass_index);
        ResourceState current_state = pass.texture_usages[handle.index].state;
        bool need_barrier = (last_state != current_state) || WasTextureWrittenByPreviousPass(handle, pass_index);

        if (need_barrier)
        {
            TextureBarrierDesc tb;
            tb.texture = m_textures[handle.index].actual_texture;
            tb.src_state = last_state;
            tb.dst_state = current_state;
            tb.layer_count = m_textures[handle.index].actual_texture->m_array_layer;
            tb.mip_level_count = m_textures[handle.index].actual_texture->mip_map_level;
            // TODO(hyl): support specify view mip/layer in framegraph

            barrier.texture_memory_barriers.push_back(tb);
        }
        processed_textures.insert(handle.index);
    }

    // Process read textures that weren't written to
    for (auto handle : pass.read_textures)
    {
        if (processed_textures.find(handle.index) != processed_textures.end())
            continue;

        ResourceState last_state = GetLastState(handle, pass_index);
        ResourceState current_state = pass.texture_usages[handle.index].state;

        if (last_state != current_state && last_state != ResourceState::RESOURCE_STATE_UNDEFINED)
        {
            TextureBarrierDesc tb;
            tb.texture = m_textures[handle.index].actual_texture;
            tb.src_state = last_state;
            tb.dst_state = current_state;
            tb.layer_count = m_textures[handle.index].actual_texture->m_array_layer;
            tb.mip_level_count = m_textures[handle.index].actual_texture->mip_map_level;
            // TODO(hyl): ditto
            barrier.texture_memory_barriers.push_back(tb);
        }
        processed_textures.insert(handle.index);
    }

    // Process buffer barriers - prioritize writes over reads for same resource
    for (auto handle : pass.write_buffers)
    {
        if (processed_buffers.find(handle.index) != processed_buffers.end())
            continue;

        ResourceState last_state = GetLastState(handle, pass_index);
        ResourceState current_state = pass.buffer_usages[handle.index].state;
        bool need_barrier = (last_state != current_state) || WasBufferWrittenByPreviousPass(handle, pass_index);

        if (need_barrier)
        {
            BufferBarrierDesc bb;
            bb.buffer = m_buffers[handle.index].actual_buffer;
            bb.src_state = last_state;
            bb.dst_state = current_state;
            barrier.buffer_memory_barriers.push_back(bb);
        }
        processed_buffers.insert(handle.index);
    }

    // Process read buffers that weren't written to
    for (auto handle : pass.read_buffers)
    {
        if (processed_buffers.find(handle.index) != processed_buffers.end())
            continue;

        ResourceState last_state = GetLastState(handle, pass_index);
        ResourceState current_state = pass.buffer_usages[handle.index].state;
        bool need_barrier = WasBufferWrittenByPreviousPass(handle, pass_index) ||
                            (last_state != current_state && last_state != ResourceState::RESOURCE_STATE_UNDEFINED);

        if (need_barrier)
        {
            BufferBarrierDesc bb;
            bb.buffer = m_buffers[handle.index].actual_buffer;
            bb.src_state = last_state;
            bb.dst_state = current_state;
            barrier.buffer_memory_barriers.push_back(bb);
        }
        processed_buffers.insert(handle.index);
    }

    // Insert barrier if needed
    if (!barrier.texture_memory_barriers.empty() || !barrier.buffer_memory_barriers.empty())
    {
        command_list->InsertBarrier(barrier);
        // for (auto& barrier : barrier.texture_memory_barriers)
        // {
        //     LOG_DEBUG("barrier info: srcstate {} dststate{} tex {} depth {} arraylayer{}" , (u32)barrier.src_state,
        //     (u32)barrier.dst_state, barrier.texture->m_debug_name, barrier.texture->m_depth, barrier.layer_count);
        // }
    }
}

ResourceState FrameGraph::GetLastState(TextureHandle handle, u32 pass_index)
{
    // Find the last pass that used this texture in execution order
    // pass_index is an index into m_execution_order, not m_passes
    // Priority: write operations change resource state, so check writes first
    ResourceState usage = ResourceState::RESOURCE_STATE_UNDEFINED;
    i32 passindex = -1;
    for (i32 i = static_cast<i32>(pass_index) - 1; i >= 0; --i)
    {
        u32 actual_pass_idx = m_execution_order[i];
        auto &pass = m_passes[actual_pass_idx];

        // First check if this pass wrote to the texture (write operations change state)
        for (auto write_handle : pass.write_textures)
        {
            if (write_handle.index == handle.index)
            {
                usage = pass.texture_usages[handle.index].state;
                passindex = i;
                break;
            }
        }
    }

    // If no write found, check for reads
    for (i32 i = static_cast<i32>(pass_index) - 1; i >= 0; --i)
    {
        u32 actual_pass_idx = m_execution_order[i];
        auto &pass = m_passes[actual_pass_idx];

        // Check if this pass read from the texture
        for (auto read_handle : pass.read_textures)
        {
            if (read_handle.index == handle.index)
            {
                if (i > passindex)
                {
                    return pass.texture_usages[handle.index].state;
                }
            }
        }
    }

    return usage != ResourceState::RESOURCE_STATE_UNDEFINED ? usage : ResourceState::RESOURCE_STATE_UNDEFINED;
}

ResourceState FrameGraph::GetLastState(BufferHandle handle, u32 pass_index)
{
    // Find the last pass that used this buffer in execution order
    // pass_index is an index into m_execution_order, not m_passes
    // Priority: write operations change resource state, so check writes first
    for (i32 i = static_cast<i32>(pass_index) - 1; i >= 0; --i)
    {
        u32 actual_pass_idx = m_execution_order[i];
        auto &pass = m_passes[actual_pass_idx];

        // First check if this pass wrote to the buffer (write operations change state)
        for (auto write_handle : pass.write_buffers)
        {
            if (write_handle.index == handle.index)
            {
                return pass.buffer_usages[handle.index].state;
            }
        }
    }

    // If no write found, check for reads
    for (i32 i = static_cast<i32>(pass_index) - 1; i >= 0; --i)
    {
        u32 actual_pass_idx = m_execution_order[i];
        auto &pass = m_passes[actual_pass_idx];

        // Check if this pass read from the buffer
        for (auto read_handle : pass.read_buffers)
        {
            if (read_handle.index == handle.index)
            {
                return pass.buffer_usages[handle.index].state;
            }
        }
    }

    return ResourceState::RESOURCE_STATE_UNDEFINED;
}

bool FrameGraph::WasTextureWrittenByPreviousPass(TextureHandle handle, u32 pass_index)
{
    for (i32 i = static_cast<i32>(pass_index) - 1; i >= 0; --i)
    {
        u32 actual_pass_idx = m_execution_order[i];
        auto &p = m_passes[actual_pass_idx];
        for (auto write_handle : p.write_textures)
        {
            if (write_handle.index == handle.index)
                return true;
        }
    }
    return false;
}

bool FrameGraph::WasBufferWrittenByPreviousPass(BufferHandle handle, u32 pass_index)
{
    for (i32 i = static_cast<i32>(pass_index) - 1; i >= 0; --i)
    {
        u32 actual_pass_idx = m_execution_order[i];
        auto &p = m_passes[actual_pass_idx];
        for (auto write_handle : p.write_buffers)
        {
            if (write_handle.index == handle.index)
                return true;
        }
    }
    return false;
}

} // namespace Horizon::Backend
