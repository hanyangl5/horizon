/*****************************************************************//**
 * \file   resource_barrier.h
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/buffer.h>
#include <runtime/function/rhi/texture.h>

namespace Horizon {

enum QueueOp { IGNORED, RELEASE, ACQUIRE };

struct BufferBarrierDesc {
    Backend::Buffer *buffer{};
    u64 size{};
    u64 offset{};
    ResourceState src_state{}, dst_state{};
    //CommandQueueType queue{}; // only the other queue type is need
    //QueueOp queue_op{QueueOp::IGNORED};
};

struct TextureBarrierDesc {
    Backend::Texture *texture;
    ResourceState src_state{}, dst_state{};
    u32 first_mip_level{};
    u32 mip_level_count{1};
    u32 first_layer{};
    u32 layer_count{1};
    //CommandQueueType queue; // only the other queue type is need
    //QueueOp queue_op{QueueOp::IGNORED};
};

struct BarrierDesc {
    Container::Array<BufferBarrierDesc> buffer_memory_barriers{};
    Container::Array<TextureBarrierDesc> texture_memory_barriers{};
};
} // namespace Horizon