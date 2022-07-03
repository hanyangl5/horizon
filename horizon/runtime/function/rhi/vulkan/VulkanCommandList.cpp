#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanCommandList.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>

namespace Horizon::RHI {

VulkanCommandList::VulkanCommandList(CommandQueueType type,
                                     VkCommandBuffer command_buffer) noexcept
    : CommandList(type), m_command_buffer(command_buffer) {}

VulkanCommandList::~VulkanCommandList() noexcept { delete m_stage_buffer; }

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
                         VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
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

    // for (auto& cmdbuf : m_command_context_map) {
    // 	//m_vulkan.secondary_command_buffer.emplace_back(cmdbuf.second->);
    // }

    // vkCmdExecuteCommands(m_vulkan.primary_command_buffer,
    // m_vulkan.secondary_command_buffer.size(),
    // m_vulkan.secondary_command_buffer.data());

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

    assert(buffer->GetBufferSize() == size);

    // cannot update static buffer more than once
    // TODO: refractor
    bool &initialized{buffer->Initialized()};
    if (!(buffer->GetBufferUsage() &
          BufferUsage::BUFFER_USAGE_DYNAMIC_UPDATE) &&
        !initialized) {
        LOG_ERROR("buffer {} is a static buffer and is initialized",
                  (void *)buffer);
        return;
    }

    // frequently updated buffer
    {
        initialized = true;
        auto vk_buffer = dynamic_cast<VulkanBuffer *>(buffer);

        VulkanBuffer *stage_buffer = GetStageBuffer(
            vk_buffer->m_allocator,
            BufferCreateInfo{BufferUsage::BUFFER_USAGE_TRANSFER_SRC, size});

        if (memcmp(stage_buffer->m_allocation_info.pMappedData, data, size) ==
            0) {
            LOG_DEBUG("prev buffer and current buffer are same");
            return;
        }

        memcpy(stage_buffer->m_allocation_info.pMappedData, data, size);

        // barrier 1
        {
            BufferMemoryBarrierDesc bmb{};
            bmb.src_access_mask = MemoryAccessFlags::ACCESS_HOST_WRITE_BIT;
            bmb.dst_access_mask = MemoryAccessFlags::ACCESS_TRANSFER_READ_BIT;
            bmb.buffer = stage_buffer->GetBufferPointer();
            bmb.offset = 0;
            bmb.size = stage_buffer->GetBufferSize();

            BarrierDesc desc{};
            desc.src_stage = PipelineStageFlags::PIPELINE_STAGE_HOST_BIT;
            desc.dst_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
            desc.buffer_memory_barriers.emplace_back(bmb);

            InsertBarrier(desc);
        }

        // copy to gpu buffer
        CopyBuffer(stage_buffer, vk_buffer);
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
    auto vk_src_buffer = dynamic_cast<VulkanBuffer *>(src_buffer);
    auto vk_dst_buffer = dynamic_cast<VulkanBuffer *>(dst_buffer);
    CopyBuffer(vk_src_buffer, vk_dst_buffer);
}

void VulkanCommandList::CopyBuffer(VulkanBuffer *src_buffer,
                                   VulkanBuffer *dst_buffer) noexcept {
    assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
    VkBufferCopy region{};
    region.size = dst_buffer->GetBufferSize();
    vkCmdCopyBuffer(m_command_buffer, src_buffer->m_buffer,
                    dst_buffer->m_buffer, 1, &region);
}

void VulkanCommandList::UpdateTexture() noexcept {
    if (!is_recoring) {
        LOG_ERROR("command buffer isn't recording");
        return;
    }
    if (m_type != CommandQueueType::TRANSFER) {
        LOG_ERROR("invalid commands for current commandlist, expect transfer "
                  "commandlist");
    }
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

    VkPipelineStageFlags src_stage = ToVkPipelineStage(desc.src_stage);
    VkPipelineStageFlags dst_stage = ToVkPipelineStage(desc.dst_stage);

    std::vector<VkBufferMemoryBarrier> buffer_memory_barriers(
        desc.buffer_memory_barriers.size());
    std::vector<VkImageMemoryBarrier> image_memory_barriers(
        desc.image_memory_barriers.size());

    for (u32 i = 0; i < desc.buffer_memory_barriers.size(); i++) {
        buffer_memory_barriers[i].sType =
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buffer_memory_barriers[i].srcAccessMask = ToVkMemoryAccessFlags(
            desc.buffer_memory_barriers[i].src_access_mask);
        buffer_memory_barriers[i].dstAccessMask = ToVkMemoryAccessFlags(
            desc.buffer_memory_barriers[i].dst_access_mask);
        buffer_memory_barriers[i].buffer =
            static_cast<VkBuffer>(desc.buffer_memory_barriers[i].buffer);
        buffer_memory_barriers[i].offset =
            desc.buffer_memory_barriers[i].offset;
        buffer_memory_barriers[i].size = desc.buffer_memory_barriers[i].size;
    }

    // for (u32 i = 0; i < desc.image_memory_barriers.size(); i++) {
    //	image_memory_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //	image_memory_barriers[i].srcAccessMask =
    //ToVkMemoryAccessFlags(desc.image_memory_barriers[i].src_access_mask);
    //	image_memory_barriers[i].dstAccessMask =
    //ToVkMemoryAccessFlags(desc.image_memory_barriers[i].dst_access_mask);
    //	image_memory_barriers[i].oldLayout =
    //ToVkImageLayout(desc.image_memory_barriers[i].src_usage);
    //	image_memory_barriers[i].newLayout = ToVkImageLaconst Pipeline&
    //pipelineyout(desc.image_memory_barriers[i].dst_usage);
    //	image_memory_barriers[i].image =
    //desc.image_memory_barriers[i].texture->GetImage();
    //	image_memory_barriers[i].subresourceRange =
    //desc.image_memory_barriers[i].texture->GetSubresourceRange();

    //}

    vkCmdPipelineBarrier(
        m_command_buffer, src_stage, dst_stage, 0, 0, nullptr,
        desc.buffer_memory_barriers.size(), buffer_memory_barriers.data(),
        desc.image_memory_barriers.size(), image_memory_barriers.data());
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
    auto vk_pipeline = dynamic_cast<VulkanPipeline *>(pipeline);
    VkPipelineBindPoint bind_point = ToVkPipelineBindPoint(pipeline->GetType());
    vk_pipeline->Create();
    vkCmdBindPipeline(m_command_buffer, bind_point, vk_pipeline->m_pipeline);
}

VulkanBuffer *VulkanCommandList::GetStageBuffer(
    VmaAllocator allocator,
    const BufferCreateInfo &buffer_create_info) noexcept {
    if (m_stage_buffer) {
        return m_stage_buffer;
    } else {
        m_stage_buffer = new VulkanBuffer(allocator, buffer_create_info,
                                          MemoryFlag::CPU_VISABLE_MEMORY);
        return m_stage_buffer;
    }
}

} // namespace Horizon::RHI
