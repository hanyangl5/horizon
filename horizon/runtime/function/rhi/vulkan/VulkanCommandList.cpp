#include "VulkanCommandList.h"

#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanRenderTarget.h>

namespace Horizon::Backend {

VulkanCommandList::VulkanCommandList(const VulkanRendererContext &context, CommandQueueType type,
                                     VkCommandBuffer command_buffer) noexcept
    : CommandList(type), m_context(context), m_command_buffer(command_buffer) {}

VulkanCommandList::~VulkanCommandList() noexcept {}

void VulkanCommandList::BeginRecording() {
    is_recoring = true;
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_command_buffer, &command_buffer_begin_info);
}

void VulkanCommandList::EndRecording() {
    is_recoring = false;
    vkEndCommandBuffer(m_command_buffer);
}

void VulkanCommandList::BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets) {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));

    auto stack_memory = Memory::GetStackMemoryResource(1024);

    Container::Array<VkBuffer> vk_buffers(buffer_count, &stack_memory);
    Container::Array<VkDeviceSize> vk_offsets(buffer_count, &stack_memory);
    for (u32 i = 0; i < buffer_count; i++) {
        assert(("vertex buffer not valid",
                buffers[i]->m_descriptor_types & DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER));
        vk_buffers[i] = reinterpret_cast<VulkanBuffer *>(buffers[i])->m_buffer;
        vk_offsets[i] = offsets[i];
    }

    vkCmdBindVertexBuffers(m_command_buffer, 0, buffer_count, vk_buffers.data(), vk_offsets.data());
}

void VulkanCommandList::BindIndexBuffer(Buffer *buffer, u32 offset) {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));
    assert(("index buffer not valid", buffer->m_descriptor_types & DescriptorType::DESCRIPTOR_TYPE_INDEX_BUFFER));

    auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

    vkCmdBindIndexBuffer(m_command_buffer, vk_buffer->m_buffer, offset, VkIndexType::VK_INDEX_TYPE_UINT32);
}

// graphics commands
void VulkanCommandList::BeginRenderPass(const RenderPassBeginInfo &begin_info) {

    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));

    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.flags = 0;
    info.pNext = nullptr;
    info.layerCount = 1;
    info.viewMask = 0;

    info.renderArea =
        VkRect2D{VkOffset2D{static_cast<int>(begin_info.render_area.x), static_cast<int>(begin_info.render_area.y)},
                 VkExtent2D{begin_info.render_area.w, begin_info.render_area.h}};

    auto stack_memory = Memory::GetStackMemoryResource(1024);

    Container::Array<VkRenderingAttachmentInfo> color_attachment_info(&stack_memory);
    VkRenderingAttachmentInfo depth_attachment_info{};
    u32 color_render_target_count = 0;
    while (begin_info.render_targets[++color_render_target_count].data != nullptr)
        ;
    color_attachment_info.resize(color_render_target_count);

    for (u32 i = 0; i < color_render_target_count; i++) {
        auto t = reinterpret_cast<VulkanTexture *>(begin_info.render_targets[i].data->GetTexture());
        color_attachment_info[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment_info[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment_info[i].imageView = t->m_image_view;
        color_attachment_info[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkClearValue clear_value;
        auto &cc = std::get<ClearColorValue>(begin_info.render_targets[i].clear_color);
        memcpy(&clear_value.color, &cc, 4 * 4);
        color_attachment_info[i].clearValue = clear_value;
    }

    info.colorAttachmentCount = color_attachment_info.size();
    info.pColorAttachments = color_attachment_info.data();

    if (begin_info.depth_stencil.data) {
        auto t = reinterpret_cast<VulkanTexture *>(begin_info.depth_stencil.data->GetTexture());

        depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment_info.imageView = t->m_image_view;
        depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkClearValue clear_value;
        auto &cc = std::get<ClearValueDepthStencil>(begin_info.depth_stencil.clear_color);
        clear_value.depthStencil = {cc.depth, cc.stencil};
        depth_attachment_info.clearValue = clear_value;
        info.pDepthAttachment = &depth_attachment_info;
    }

    // not use stencil attachment yet
    // if (begin_info.stencil.data) {
    //    VkRenderingAttachmentInfo stencil_attachment_info{};
    //    stencil_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    //    stencil_attachment_info.imageLayout;
    //    stencil_attachment_info.imageView;
    //    stencil_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //    stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //    stencil_attachment_info.clearValue;
    //    info.pStencilAttachment = &stencil_attachment_info;
    //}

    vkCmdBeginRendering(m_command_buffer, &info);
}

void VulkanCommandList::EndRenderPass() {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));

    vkCmdEndRendering(m_command_buffer);
}

void VulkanCommandList::DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance) {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));
    vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void VulkanCommandList::DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count,
                                             u32 first_instance) {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));

    vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, first_index, first_vertex, first_instance);
}

void VulkanCommandList::DrawIndirect() {}

void VulkanCommandList::DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride) {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect graphics "
            "commandlist",
            m_type == CommandQueueType::GRAPHICS));
    vkCmdDrawIndexedIndirect(m_command_buffer, reinterpret_cast<VulkanBuffer *>(buffer)->m_buffer, offset, draw_count,
                             stride);
}

// compute commands
void VulkanCommandList::Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect compute "
            "commandlist",
            (m_type == CommandQueueType::COMPUTE || m_type == CommandQueueType::GRAPHICS)));

    vkCmdDispatch(m_command_buffer, group_count_x, group_count_y, group_count_z);
}
void VulkanCommandList::DispatchIndirect() {
    assert(("command list is not recording", is_recoring == true));
    assert(("invalid commands for current commandlist, expect compute "
            "commandlist",
            m_type == CommandQueueType::COMPUTE));
}

// transfer commands
void VulkanCommandList::UpdateBuffer(Buffer *buffer, void *data, u64 size) {
    assert(("command list is not recording", is_recoring == true));
    assert(buffer->m_size >= size);

    // cannot update static buffer more than once
    // TODO: refractor with new RAII resource

    // frequently updated buffer
    {

        auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);
        if (!vk_buffer->m_stage_buffer) {

            vk_buffer->m_stage_buffer =
                Memory::Alloc<VulkanBuffer>(m_context,
                                               BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_UNDEFINED,
                                                                ResourceState::RESOURCE_STATE_COPY_SOURCE, buffer->m_size},
                                               MemoryFlag::CPU_VISABLE_MEMORY);
        }
        // if cpu data does not change, don't upload // buffer handle is
        // different?
        // if (memcmp(stage_buffer->m_allocation_info.pMappedData, data, size)
        // ==
        //    0) {
        //    LOG_DEBUG("prev buffer and current buffer are same");
        //    return;
        //}

        memcpy(vk_buffer->m_stage_buffer->m_allocation_info.pMappedData, data, size);

        // barrier for stage upload
        {
            BufferBarrierDesc bmb{};
            bmb.buffer = vk_buffer->m_stage_buffer;
            bmb.src_state = RESOURCE_STATE_HOST_WRITE;
            bmb.dst_state = RESOURCE_STATE_COPY_SOURCE;

            BarrierDesc desc{};

            desc.buffer_memory_barriers.emplace_back(bmb);
            InsertBarrier(desc);
        }

        // copy to gpu buffer
        CopyBuffer(vk_buffer->m_stage_buffer, vk_buffer);

        // release queue ownership barrier, controlled by render graph?
    }
}

void VulkanCommandList::CopyBuffer(Buffer *src_buffer, Buffer *dst_buffer) {
    assert(("command list is not recording", is_recoring == true));
    // assert(("invalid commands for current commandlist, expect transfer "
    //         "commandlist",
    //         m_type == CommandQueueType::TRANSFER));

    auto vk_src_buffer = reinterpret_cast<VulkanBuffer *>(src_buffer);
    auto vk_dst_buffer = reinterpret_cast<VulkanBuffer *>(dst_buffer);
    CopyBuffer(vk_src_buffer, vk_dst_buffer);
}

void VulkanCommandList::CopyTexture(Texture *src_texture, Texture *dst_texture) {
    assert(("command list is not recording", is_recoring == true));
    // assert(("invalid commands for current commandlist, expect transfer "
    //         "commandlist",
    //         m_type == CommandQueueType::TRANSFER));
    auto vk_src_texture = reinterpret_cast<VulkanTexture *>(src_texture);
    auto vk_dst_texture = reinterpret_cast<VulkanTexture *>(dst_texture);
    CopyTexture(vk_src_texture, vk_dst_texture);
}

void VulkanCommandList::CopyBuffer(VulkanBuffer *src_buffer, VulkanBuffer *dst_buffer) {
    assert(dst_buffer->m_size == src_buffer->m_size);
    VkBufferCopy region{};
    region.size = dst_buffer->m_size;
    vkCmdCopyBuffer(m_command_buffer, src_buffer->m_buffer, dst_buffer->m_buffer, 1, &region);
}

void VulkanCommandList::CopyTexture(VulkanTexture *src_texture, VulkanTexture *dst_texture) {
    VkImageCopy cregion;
    cregion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cregion.srcSubresource.mipLevel = 0;
    cregion.srcSubresource.baseArrayLayer = 0;
    cregion.srcSubresource.layerCount = 1;
    cregion.srcOffset.x = 0;
    cregion.srcOffset.y = 0;
    cregion.srcOffset.z = 0;
    cregion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cregion.dstSubresource.mipLevel = 0;
    cregion.dstSubresource.baseArrayLayer = 0;
    cregion.dstSubresource.layerCount = 1;
    cregion.dstOffset.x = 0;
    cregion.dstOffset.y = 0;
    cregion.dstOffset.z = 0;
    cregion.extent.width = src_texture->m_width;
    cregion.extent.height = src_texture->m_height;
    cregion.extent.depth = 1;
    vkCmdCopyImage(m_command_buffer, src_texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_texture->m_image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cregion);
}

// void VulkanCommandList::CopyBufferToTexture(VulkanBuffer *src_buffer, VulkanTexture *dst_texture) {}

// void VulkanCommandList::CopyBufferToTexture(VulkanBuffer *src_buffer, VulkanTexture *dst_texture) {
//     //vkCmdCopyBufferToImage();
// }
//
// void VulkanCommandList::CopyImageToBuffer(VulkanTexture *src_texture, VulkanBuffer *dst_buffer) {
//     // vkCmdCopyBufferToImage();
// }
void VulkanCommandList::UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data) {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }


    auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

    VkDeviceSize texture_size =
        texture_data.size != 0 ? texture_data.size : texture_data.texture_data_desc->raw_data.size();

    if (!vk_texture->m_stage_buffer) {

        vk_texture->m_stage_buffer =
            Memory::Alloc<VulkanBuffer>(m_context,
                                           BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_UNDEFINED,
                                                            ResourceState::RESOURCE_STATE_COPY_SOURCE, texture_size},
                                           MemoryFlag::CPU_VISABLE_MEMORY);
    }
    memcpy(vk_texture->m_stage_buffer->m_allocation_info.pMappedData, texture_data.texture_data_desc->raw_data.data(),
           static_cast<u64>(texture_size));

    // barrier 1
    {

        // transition layout, undefined -> transfer dst
        TextureBarrierDesc tmb{};
        tmb.texture = texture;
        tmb.src_state = RESOURCE_STATE_UNDEFINED;
        tmb.dst_state = RESOURCE_STATE_COPY_DEST;
        tmb.first_mip_level = texture_data.first_mip_level;
        tmb.mip_level_count = texture_data.mip_level_count;
        tmb.first_layer = texture_data.first_layer;
        tmb.layer_count = texture_data.layer_count;
        // barrier for stage upload
        BufferBarrierDesc bmb{};
        bmb.buffer = vk_texture->m_stage_buffer;
        bmb.src_state = RESOURCE_STATE_HOST_WRITE;
        bmb.dst_state = RESOURCE_STATE_COPY_SOURCE;

        BarrierDesc desc{};

        desc.buffer_memory_barriers.emplace_back(bmb);
        desc.texture_memory_barriers.emplace_back(tmb);

        InsertBarrier(desc);
    }

    auto stack_memory = Memory::GetStackMemoryResource(4096);

    Container::Array<VkBufferImageCopy> regions(&stack_memory);

    for (u32 layer = texture_data.first_layer; layer < texture_data.first_layer + texture_data.layer_count; layer++) {

        for (u32 mip = texture_data.first_mip_level; mip < texture_data.first_mip_level + texture_data.mip_level_count;
             mip++) {
            VkBufferImageCopy region{};
            region.bufferOffset = !texture_data.texture_data_desc->data_offset_map.empty()
                                      ? texture_data.texture_data_desc->data_offset_map[layer][mip]
                                      : 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = mip; // update level 0
            region.imageSubresource.baseArrayLayer = layer;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {texture->m_width >> mip, texture->m_height >> mip, texture->m_depth};
            regions.push_back(region);
        }
    }

    // TODO : use CopyBufferToTexture
    vkCmdCopyBufferToImage(m_command_buffer, vk_texture->m_stage_buffer->m_buffer, vk_texture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<u32>(regions.size()), regions.data());
}

void VulkanCommandList::InsertBarrier(const BarrierDesc &desc) {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }

    VkAccessFlags src_access_flags = 0;
    VkAccessFlags dst_access_flags = 0;

    auto stack_memory = Memory::GetStackMemoryResource(4096);

    Container::Array<VkBufferMemoryBarrier> buffer_memory_barriers(desc.buffer_memory_barriers.size(), &stack_memory);
    Container::Array<VkImageMemoryBarrier> texture_memory_barriers(desc.texture_memory_barriers.size(), &stack_memory);

    for (u32 i = 0; i < desc.buffer_memory_barriers.size(); i++) {
        const auto &barrier_desc = desc.buffer_memory_barriers[i];
        auto &barrier = buffer_memory_barriers[i];
        const auto &vk_buffer = reinterpret_cast<VulkanBuffer *>(barrier_desc.buffer);

        if (RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.src_state &&
            RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.dst_state) {
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; //-V522
            barrier.pNext = NULL;

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        } else {
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext = NULL;

            barrier.srcAccessMask = util_to_vk_access_flags(barrier_desc.src_state);
            barrier.dstAccessMask = util_to_vk_access_flags(barrier_desc.dst_state);
        }

        if (1) {
            barrier.buffer = vk_buffer->m_buffer;
            barrier.size = VK_WHOLE_SIZE;
            barrier.offset = 0;

            if (barrier_desc.queue_op == QueueOp::ACQUIRE) {
                barrier.srcQueueFamilyIndex = m_context.command_queue_familiy_indices[barrier_desc.queue];
                barrier.dstQueueFamilyIndex = m_context.command_queue_familiy_indices[m_type];
            } else if (barrier_desc.queue_op == QueueOp::RELEASE) {
                barrier.srcQueueFamilyIndex = m_context.command_queue_familiy_indices[m_type];
                barrier.dstQueueFamilyIndex = m_context.command_queue_familiy_indices[barrier_desc.queue];
            } else {
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            src_access_flags |= barrier.srcAccessMask;
            dst_access_flags |= barrier.dstAccessMask;
        }
    }

    for (u32 i = 0; i < desc.texture_memory_barriers.size(); i++) {
        const auto &barrier_desc = desc.texture_memory_barriers[i];
        auto &barrier = texture_memory_barriers[i];
        const auto &vk_texture = reinterpret_cast<VulkanTexture *>(barrier_desc.texture);

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        if (RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.src_state &&
            RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.dst_state) {

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        } else {

            barrier.srcAccessMask = util_to_vk_access_flags(barrier_desc.src_state);
            barrier.dstAccessMask = util_to_vk_access_flags(barrier_desc.dst_state);
            barrier.oldLayout = util_to_vk_image_layout(barrier_desc.src_state);
            barrier.newLayout = util_to_vk_image_layout(barrier_desc.dst_state);
        }

        if (1) {

            barrier.image = vk_texture->m_image;

            if (vk_texture->m_format == TextureFormat::TEXTURE_FORMAT_DUMMY_COLOR) {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            } else {
                barrier.subresourceRange.aspectMask = ToVkAspectMaskFlags(ToVkImageFormat(vk_texture->m_format), false);
            }
            barrier.subresourceRange.baseMipLevel = barrier_desc.first_mip_level;
            barrier.subresourceRange.levelCount = barrier_desc.mip_level_count;
            // todo
            barrier.subresourceRange.baseArrayLayer = barrier_desc.first_layer;
            barrier.subresourceRange.layerCount = barrier_desc.layer_count;
            // barrier.subresourceRange.aspectMask =
            //     (VkImageAspectFlags)pTexture->mAspectMask;

            // barrier.subresourceRange.baseArrayLayer =
            //      barrier_desc.mSubresourceBarrier ? barrier_desc.mArrayLayer :
            //      0;
            //  barrier.subresourceRange.layerCount =
            //      barrier_desc.mSubresourceBarrier ? 1 :
            //      VK_REMAINING_ARRAY_LAYERS;

            if (barrier_desc.queue_op == QueueOp::ACQUIRE) {
                ;
                barrier.srcQueueFamilyIndex = m_context.command_queue_familiy_indices[barrier_desc.queue];
                barrier.dstQueueFamilyIndex = m_context.command_queue_familiy_indices[m_type];
            } else if (barrier_desc.queue_op == QueueOp::RELEASE) {
                barrier.srcQueueFamilyIndex = m_context.command_queue_familiy_indices[m_type];
                barrier.dstQueueFamilyIndex = m_context.command_queue_familiy_indices[barrier_desc.queue];
            } else {
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            src_access_flags |= barrier.srcAccessMask;
            dst_access_flags |= barrier.dstAccessMask;
        }

        src_access_flags |= barrier.srcAccessMask;
        dst_access_flags |= barrier.dstAccessMask;
    }

    VkPipelineStageFlags src_stage_flags{util_determine_pipeline_stage_flags(src_access_flags, m_type)};
    VkPipelineStageFlags dst_stage_flags{util_determine_pipeline_stage_flags(dst_access_flags, m_type)};

    if (!buffer_memory_barriers.empty() || !texture_memory_barriers.empty()) {
        vkCmdPipelineBarrier(m_command_buffer, src_stage_flags, dst_stage_flags, 0, 0, nullptr,
                             buffer_memory_barriers.size(), buffer_memory_barriers.data(),
                             texture_memory_barriers.size(), texture_memory_barriers.data());
    }
}

void VulkanCommandList::BindPipeline(Pipeline *pipeline) {
    assert(("command list is not recording", is_recoring == true));
    if (pipeline->GetType() == PipelineType::GRAPHICS || pipeline->GetType() == PipelineType::RAY_TRACING) {
        assert(("pipeline type does not correspond with current command list, "
                "expect compute pipeline",
                m_type == CommandQueueType::GRAPHICS));
    } else if (pipeline->GetType() == PipelineType::COMPUTE) {
        assert(("pipeline type does not correspond with current command list, "
                "expect compute pipeline",
                m_type == CommandQueueType::COMPUTE));
    } else {
        assert(("cannot bind pipeline using transfer command list", m_type != CommandQueueType::TRANSFER));
    }

    auto vk_pipeline = reinterpret_cast<VulkanPipeline *>(pipeline);
    VkPipelineBindPoint bind_point = ToVkPipelineBindPoint(pipeline->GetType());

    if (pipeline->GetType() == PipelineType::GRAPHICS) {
        // TOOD: set viewport and scissor manually?
        vkCmdSetViewport(m_command_buffer, 0, 1, &vk_pipeline->view_port);
        vkCmdSetScissor(m_command_buffer, 0, 1, &vk_pipeline->scissor);
    }
    vkCmdBindPipeline(m_command_buffer, bind_point, vk_pipeline->m_pipeline);
}

void VulkanCommandList::BindPushConstant(Pipeline *pipeline, const Container::String &name, void *data) {
    assert(("command list is not recording", is_recoring == true));
    assert(("cannot bind push constant using transfer command list", m_type != CommandQueueType::TRANSFER));
    auto vk_pipeline = reinterpret_cast<VulkanPipeline *>(pipeline);
    auto res = vk_pipeline->GetRootSignatureDesc().push_constants.find(name);
    if (res == vk_pipeline->GetRootSignatureDesc().push_constants.end()) {
        LOG_ERROR("pipeline doesn't have push constant {}", name);
        return;
    } else {
        vkCmdPushConstants(m_command_buffer, vk_pipeline->m_pipeline_layout,
                           ToVkShaderStageFlags(res->second.shader_stages), res->second.offset, res->second.size, data);
    }
}

void VulkanCommandList::BindPushConstant(Pipeline *pipeline, u32 index, void *data) {}

void VulkanCommandList::ClearBuffer(Buffer *buffer, f32 clear_value) {
    assert(("command list is not recording", is_recoring == true));
    assert(("clear buffer can only call by transfer command list", m_type == CommandQueueType::TRANSFER));
    // vkCmdFillBuffer();
}

void VulkanCommandList::ClearTextrue(Texture *texture, const Math::float4 &clear_value) {
    assert(("command list is not recording", is_recoring == true));
    assert(("clear texture can only call by transfer command list", m_type == CommandQueueType::TRANSFER));
    // vkCmdClearColorImage();
}

void VulkanCommandList::BindDescriptorSets(Pipeline *pipeline, DescriptorSet *set) {
    assert(("command list is not recording", is_recoring == true));
    auto vk_pipeline = reinterpret_cast<VulkanPipeline *>(pipeline);
    VkPipelineBindPoint bind_point = ToVkPipelineBindPoint(pipeline->GetType());

    auto vk_set = reinterpret_cast<VulkanDescriptorSet *>(set);

    vkCmdBindDescriptorSets(m_command_buffer, bind_point, vk_pipeline->m_pipeline_layout,
                            static_cast<u32>(set->update_frequency), 1, &vk_set->m_set, 0, 0); // TODO: batch update
}

void VulkanCommandList::GenerateMipMap(Texture *texture, bool alllevels) {

    if(texture->mip_map_level == 1) return;

    auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

    i32 mip_w = texture->m_width, mip_h = texture->m_height;
    
    for (u32 i = 1; i < texture->mip_map_level; i++) {
        {
            BarrierDesc desc{};
            TextureBarrierDesc mip_map_barrier{};
            mip_map_barrier.texture = texture;
            mip_map_barrier.first_mip_level = i;
            mip_map_barrier.mip_level_count = 1;
            mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
            desc.texture_memory_barriers.emplace_back(mip_map_barrier);

            InsertBarrier(desc);
        }
        {
            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_w, mip_h, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mip_w > 1 ? mip_w / 2 : 1, mip_h > 1 ? mip_h / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(m_command_buffer, vk_texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           vk_texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        }
        {
            BarrierDesc desc{};
            TextureBarrierDesc mip_map_barrier{};
            mip_map_barrier.texture = texture;
            mip_map_barrier.first_mip_level = i;
            mip_map_barrier.mip_level_count = 1;
            mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
            mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
            desc.texture_memory_barriers.emplace_back(mip_map_barrier);
            InsertBarrier(desc);
        }
        if (mip_w > 1)
            mip_w /= 2;
        if (mip_h > 1)
            mip_h /= 2;
    }

}

VulkanBuffer* VulkanCommandList::GetStageBuffer(const BufferCreateInfo &buffer_create_info) {
    return Memory::Alloc<VulkanBuffer>(m_context, buffer_create_info, MemoryFlag::CPU_VISABLE_MEMORY);
}

} // namespace Horizon::Backend
