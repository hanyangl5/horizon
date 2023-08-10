#include "ResourceBarrier.h"

#include <vector>

namespace Horizon {

void InsertBarrier(u32 i, std::shared_ptr<CommandBuffer> command_buffer, const BarrierDesc &desc) noexcept {
    VkPipelineStageFlags src_stage = ToVkPipelineStage(desc.src_stage);
    VkPipelineStageFlags dst_stage = ToVkPipelineStage(desc.dst_stage);

    std::vector<VkBufferMemoryBarrier> buffer_memory_barriers(desc.buffer_memory_barriers.size());
    std::vector<VkImageMemoryBarrier> image_memory_barriers(desc.image_memory_barriers.size());

    for (u32 i = 0; i < desc.buffer_memory_barriers.size(); i++) {
        buffer_memory_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buffer_memory_barriers[i].srcAccessMask = ToVkMemoryAccessFlags(desc.buffer_memory_barriers[i].src_access_mask);
        buffer_memory_barriers[i].dstAccessMask = ToVkMemoryAccessFlags(desc.buffer_memory_barriers[i].dst_access_mask);
        buffer_memory_barriers[i].buffer = static_cast<VkBuffer>(desc.buffer_memory_barriers[i].buffer);
        buffer_memory_barriers[i].offset = desc.buffer_memory_barriers[i].offset;
        buffer_memory_barriers[i].size = desc.buffer_memory_barriers[i].size;
    }

    for (u32 i = 0; i < desc.image_memory_barriers.size(); i++) {
        image_memory_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barriers[i].srcAccessMask = ToVkMemoryAccessFlags(desc.image_memory_barriers[i].src_access_mask);
        image_memory_barriers[i].dstAccessMask = ToVkMemoryAccessFlags(desc.image_memory_barriers[i].dst_access_mask);
        image_memory_barriers[i].oldLayout = ToVkImageLayout(desc.image_memory_barriers[i].src_usage);
        image_memory_barriers[i].newLayout = ToVkImageLayout(desc.image_memory_barriers[i].dst_usage);
        image_memory_barriers[i].image = desc.image_memory_barriers[i].texture->GetImage();
        image_memory_barriers[i].subresourceRange = desc.image_memory_barriers[i].texture->GetSubresourceRange();
    }

    vkCmdPipelineBarrier(command_buffer->Get(i), src_stage, dst_stage, 0, 0, nullptr,
                         desc.buffer_memory_barriers.size(), buffer_memory_barriers.data(),
                         desc.image_memory_barriers.size(), image_memory_barriers.data());
}

} // namespace Horizon