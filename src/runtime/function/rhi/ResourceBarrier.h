#pragma once

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon {

enum QueueOp { IGNORED, RELEASE, ACQUIRE };

struct BufferBarrierDesc {
    Backend::Buffer *buffer{};
    // u32 offset;
    // u64 size;
    // u32 src_access_mask, dst_access_mask;
    // CommandQueueType src_queue, dst_queue;
    ResourceState src_state{}, dst_state{};
    CommandQueueType queue{}; // only the other queue type is need
    QueueOp queue_op{QueueOp::IGNORED};
};

struct TextureBarrierDesc {
    Backend::Texture *texture;
    // MemoryAccessFlags src_access_mask, dst_access_mask;
    // TextureUsage src_usage, dst_usage; // transition image layout
    ResourceState src_state{}, dst_state{};
    u32 first_mip_level{};
    u32 mip_level_count{1};
    u32 first_layer{};
    u32 layer_count{1};
    // CommandQueueType src_queue, dst_queue;
    CommandQueueType queue; // only the other queue type is need
    QueueOp queue_op{QueueOp::IGNORED};
};

struct BarrierDesc {
    // u32 src_stage, dst_stage;
    Container::Array<BufferBarrierDesc> buffer_memory_barriers{};
    Container::Array<TextureBarrierDesc> texture_memory_barriers{};
};
} // namespace Horizon