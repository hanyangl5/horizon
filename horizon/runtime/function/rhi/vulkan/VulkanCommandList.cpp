#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanCommandList.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <stb_image.h>

namespace Horizon::RHI {

VulkanCommandList::VulkanCommandList(const VulkanRendererContext &context,
                                     CommandQueueType type,
                                     VkCommandBuffer command_buffer) noexcept
    : CommandList(type), m_context(context), m_command_buffer(command_buffer) {}

VulkanCommandList::~VulkanCommandList() noexcept {
}

void VulkanCommandList::BeginRecording() noexcept {
    is_recoring = true;
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_command_buffer, &command_buffer_begin_info);
}

void VulkanCommandList::EndRecording() noexcept {
    is_recoring = false;
    vkEndCommandBuffer(m_command_buffer);
}

// graphics commands
void VulkanCommandList::BeginRenderPass() noexcept {

    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }

    if (m_type != CommandQueueType::GRAPHICS) {
        LOG_ERROR("invalid commands for current commandlist, expect graphics "
                  "commandlist");
        return;
    }
    VkRenderPassBeginInfo render_pass_begin_info;

    vkCmdBeginRenderPass(m_command_buffer, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandList::EndRenderPass() noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::GRAPHICS) {
        LOG_ERROR("invalid commands for current commandlist, expect graphics "
                  "commandlist");
        return;
    }

    vkCmdEndRenderPass(m_command_buffer);
}
void VulkanCommandList::Draw() noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::GRAPHICS) {
        LOG_ERROR("invalid commands for current commandlist, expect graphics "
                  "commandlist");
        return;
    }
}
void VulkanCommandList::DrawIndirect() noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::GRAPHICS) {
        LOG_ERROR("invalid commands for current commandlist, expect graphics "
                  "commandlist");
        return;
    }
}

// compute commands
void VulkanCommandList::Dispatch(u32 group_count_x, u32 group_count_y,
                                 u32 group_count_z) noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::COMPUTE) {
        LOG_ERROR("invalid commands for current commandlist, expect compute "
                  "commandlist");
        return;
    }

    vkCmdDispatch(m_command_buffer, group_count_x, group_count_y,
                  group_count_z);
}
void VulkanCommandList::DispatchIndirect() noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::COMPUTE) {
        LOG_ERROR("invalid commands for current commandlist, expect compute "
                  "commandlist");
        return;
    }
}

// transfer commands
void VulkanCommandList::UpdateBuffer(Buffer *buffer, void *data,
                                     u64 size) noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::TRANSFER) {
        LOG_ERROR("invalid commands for current commandlist, expect transfer "
                  "commandlist");
        return;
    }

    assert(buffer->m_size == size);

    // cannot update static buffer more than once
    // TODO: refractor with new RAII resource

    // frequently updated buffer
    {

        auto vk_buffer = static_cast<VulkanBuffer *>(buffer);
        vk_buffer->m_stage_buffer;
        vk_buffer->m_stage_buffer = GetStageBuffer(
            vk_buffer->m_allocator,
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_UNDEFINED,
                             ResourceState::RESOURCE_STATE_COPY_SOURCE, size});

        // if cpu data does not change, don't upload // buffer handle is
        // different?
        // if (memcmp(stage_buffer->m_allocation_info.pMappedData, data, size)
        // ==
        //    0) {
        //    LOG_DEBUG("prev buffer and current buffer are same");
        //    return;
        //}

        memcpy(vk_buffer->m_stage_buffer->m_allocation_info.pMappedData, data,
               size);

        // barrier for stage upload
        {
            BufferBarrierDesc bmb{};
            bmb.buffer = vk_buffer->m_stage_buffer.get();
            bmb.src_state = RESOURCE_STATE_HOST_WRITE;
            bmb.dst_state = RESOURCE_STATE_COPY_SOURCE;

            BarrierDesc desc{};

            desc.buffer_memory_barriers.emplace_back(bmb);
            InsertBarrier(desc);
        }

        // copy to gpu buffer
        CopyBuffer(vk_buffer->m_stage_buffer.get(), vk_buffer);

        // release queue ownership barrier, controlled by render graph?
    }
}

void VulkanCommandList::CopyBuffer(Buffer *src_buffer,
                                   Buffer *dst_buffer) noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::TRANSFER) {
        LOG_ERROR("invalid commands for current commandlist, expect transfer "
                  "commandlist");
        return;
    }
    auto vk_src_buffer = static_cast<VulkanBuffer *>(src_buffer);
    auto vk_dst_buffer = static_cast<VulkanBuffer *>(dst_buffer);
    CopyBuffer(vk_src_buffer, vk_dst_buffer);
}

void VulkanCommandList::CopyBuffer(VulkanBuffer *src_buffer,
                                   VulkanBuffer *dst_buffer) noexcept {
    assert(dst_buffer->m_size == src_buffer->m_size);
    VkBufferCopy region{};
    region.size = dst_buffer->m_size;
    vkCmdCopyBuffer(m_command_buffer, src_buffer->m_buffer,
                    dst_buffer->m_buffer, 1, &region);
}

void VulkanCommandList::UpdateTexture(
    Texture *texture, const TextureData &texture_data) noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::TRANSFER) {
        LOG_ERROR("invalid commands for current commandlist, expect transfer "
                  "commandlist");
    }

    // frequently updated buffer
    //{

    //    auto vk_texture = static_cast<VulkanTexture *>(texture);

    //    u64 texture_size =
    //        vk_texture->m_width * vk_texture->m_height * vk_texture->m_depth;

    //    auto& stage_buffer = GetStageBuffer(
    //        vk_texture->m_allocator,
    //        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_UNDEFINED,
    //                         ResourceState::RESOURCE_STATE_COPY_SOURCE,
    //                         texture_size});
    //    memcpy(stage_buffer->m_allocation_info.pMappedData, texture_data.data,
    //           texture_size);

    //    vmaBindImageMemory(vk_texture->m_allocator, stage_buffer->m_memory,
    //                       vk_texture->m_image);

    //    // barrier 1
    //    {
    //        // barrier for stage upload
    //        BufferBarrierDesc bmb{};
    //        bmb.buffer = stage_buffer;
    //        bmb.src_state = RESOURCE_STATE_HOST_WRITE;
    //        bmb.dst_state = RESOURCE_STATE_COPY_SOURCE;

    //        // transition layout, undefined -> transfer dst
    //        TextureBarrierDesc tmb{};
    //        tmb.texture = texture;
    //        tmb.src_state = RESOURCE_STATE_UNDEFINED;
    //        tmb.dst_state = RESOURCE_STATE_COPY_DEST;

    //        BarrierDesc desc{};

    //        desc.buffer_memory_barriers.emplace_back(bmb);
    //        desc.texture_memory_barriers.emplace_back(tmb);

    //        InsertBarrier(desc);
    //    }

    //    // copy buffer to image

    //    VkBufferImageCopy region{};
    //    region.bufferOffset = 0;
    //    region.bufferRowLength = 0;
    //    region.bufferImageHeight = 0;
    //    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //    region.imageSubresource.mipLevel = 0;
    //    region.imageSubresource.baseArrayLayer = 0;
    //    region.imageSubresource.layerCount = 1;
    //    region.imageOffset = {0, 0, 0};
    //    region.imageExtent = {texture->m_width, texture->m_height,
    //                          texture->m_depth};

    //    vkCmdCopyBufferToImage(
    //        m_command_buffer, stage_buffer->m_buffer, vk_texture->m_image,
    //        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    //    // barrier 2, transtion image layout
    //    {

    //        // transition layout, undefined -> transfer dst
    //        TextureBarrierDesc tmb{};
    //        tmb.texture = texture;
    //        tmb.src_state = RESOURCE_STATE_COPY_DEST;
    //        tmb.dst_state = RESOURCE_STATE_COPY_DEST; // usage determin

    //        BarrierDesc desc{};

    //        desc.texture_memory_barriers.emplace_back(tmb);

    //        InsertBarrier(desc);
    //    }
    //    // create sampler and image view
    //}
}

void VulkanCommandList::CopyTexture() noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::TRANSFER) {
        LOG_ERROR("invalid commands for current commandlist, expect transfer "
                  "commandlist");
    }
}

void VulkanCommandList::InsertBarrier(const BarrierDesc &desc) noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }

    VkAccessFlags src_access_flags = 0;
    VkAccessFlags dst_access_flags = 0;

    std::vector<VkBufferMemoryBarrier> buffer_memory_barriers(
        desc.buffer_memory_barriers.size());
    std::vector<VkImageMemoryBarrier> texture_memory_barriers(
        desc.texture_memory_barriers.size());

    for (u32 i = 0; i < desc.buffer_memory_barriers.size(); i++) {
        const auto &barrier_desc = desc.buffer_memory_barriers[i];
        auto &barrier = buffer_memory_barriers[i];
        const auto &vk_buffer =
            static_cast<VulkanBuffer *>(barrier_desc.buffer);

        if (RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.src_state &&
            RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.dst_state) {
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; //-V522
            barrier.pNext = NULL;

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask =
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        } else {
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext = NULL;

            barrier.srcAccessMask =
                util_to_vk_access_flags(barrier_desc.src_state);
            barrier.dstAccessMask =
                util_to_vk_access_flags(barrier_desc.dst_state);
        }

        if (1) {
            barrier.buffer = vk_buffer->m_buffer;
            barrier.size = VK_WHOLE_SIZE;
            barrier.offset = 0;

            if (barrier_desc.queue_op == QueueOp::ACQUIRE) {
                barrier.srcQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[barrier_desc.queue];
                barrier.dstQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[m_type];
            } else if (barrier_desc.queue_op == QueueOp::RELEASE) {
                barrier.srcQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[m_type];
                barrier.dstQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[barrier_desc.queue];
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
        const auto &vk_texture =
            static_cast<VulkanTexture *>(barrier_desc.texture);

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        if (RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.src_state &&
            RESOURCE_STATE_UNORDERED_ACCESS == barrier_desc.dst_state) {

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask =
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        } else {

            barrier.srcAccessMask =
                util_to_vk_access_flags(barrier_desc.src_state);
            barrier.dstAccessMask =
                util_to_vk_access_flags(barrier_desc.dst_state);
            barrier.oldLayout = util_to_vk_image_layout(barrier_desc.src_state);
            barrier.newLayout = util_to_vk_image_layout(barrier_desc.dst_state);
        }

        if (1) {

            barrier.image = vk_texture->m_image;
            barrier.subresourceRange = VkImageSubresourceRange{
                ToVkAspectMaskFlags(ToVkImageFormat(vk_texture->m_format),
                                    false),
                0, 1, 0, 1};
            // barrier.subresourceRange.aspectMask =
            //     (VkImageAspectFlags)pTexture->mAspectMask;
            // barrier.subresourceRange.baseMipLevel =
            //     barrier_desc.mSubresourceBarrier ? barrier_desc.mMipLevel :
            //     0;
            // barrier.subresourceRange.levelCount =
            //     barrier_desc.mSubresourceBarrier ? 1 :
            //     VK_REMAINING_MIP_LEVELS;
            // barrier.subresourceRange.baseArrayLayer =
            //     barrier_desc.mSubresourceBarrier ? barrier_desc.mArrayLayer :
            //     0;
            // barrier.subresourceRange.layerCount =
            //     barrier_desc.mSubresourceBarrier ? 1 :
            //     VK_REMAINING_ARRAY_LAYERS;

            if (barrier_desc.queue_op == QueueOp::ACQUIRE) {
                ;
                barrier.srcQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[barrier_desc.queue];
                barrier.dstQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[m_type];
            } else if (barrier_desc.queue_op == QueueOp::RELEASE) {
                barrier.srcQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[m_type];
                barrier.dstQueueFamilyIndex =
                    m_context.command_queue_familiy_indices[barrier_desc.queue];
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

    VkPipelineStageFlags src_stage_flags{
        util_determine_pipeline_stage_flags(src_access_flags, m_type)};
    VkPipelineStageFlags dst_stage_flags{
        util_determine_pipeline_stage_flags(dst_access_flags, m_type)};

    if (!buffer_memory_barriers.empty() || !texture_memory_barriers.empty()) {
        vkCmdPipelineBarrier(m_command_buffer, src_stage_flags, dst_stage_flags,
                             0, 0, nullptr, desc.buffer_memory_barriers.size(),
                             buffer_memory_barriers.data(),
                             desc.texture_memory_barriers.size(),
                             texture_memory_barriers.data());
    }
}

void VulkanCommandList::BindPipeline(Pipeline *pipeline) noexcept {
    switch (pipeline->GetType()) {
    case PipelineType::GRAPHICS:
    case PipelineType::RAY_TRACING:
        if (m_type != CommandQueueType::GRAPHICS) {
            LOG_ERROR("command list type not correspond with pipeline type");
            return;
        }
        break;

    case PipelineType::COMPUTE:
        if (m_type != CommandQueueType::COMPUTE) {
            LOG_ERROR("command list type not correspond with pipeline type");
            return;
        }
        break;
    default:
        return;
    }
    auto vk_pipeline = static_cast<VulkanPipeline *>(pipeline);
    VkPipelineBindPoint bind_point = ToVkPipelineBindPoint(pipeline->GetType());

    vk_pipeline->m_descriptor_set_manager.Update(); // update descriptor sets

    vkCmdBindDescriptorSets(
        m_command_buffer, bind_point, vk_pipeline->m_pipeline_layout, 0,
        vk_pipeline->m_pipeline_layout_desc.sets.size(),
        vk_pipeline->m_pipeline_layout_desc.sets.data(), 0, 0);
    vkCmdBindPipeline(m_command_buffer, bind_point, vk_pipeline->m_pipeline);
}

Resource<VulkanBuffer> VulkanCommandList::GetStageBuffer(
    VmaAllocator allocator,
    const BufferCreateInfo &buffer_create_info) noexcept {
    return std::make_unique<VulkanBuffer>(allocator, buffer_create_info,
                                          MemoryFlag::CPU_VISABLE_MEMORY);
}

} // namespace Horizon::RHI
