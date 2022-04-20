#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/log/Log.h>

namespace Horizon {

	struct RenderContext {
		u32 width;
		u32 height;
		u32 swap_chain_image_count = 3;
	};

	typedef enum DescriptorTypeFlags
	{
		DESCRIPTOR_TYPE_INVALID = 0,
		DESCRIPTOR_TYPE_UNIFORM_BUFFER = 1,
		DESCRIPTOR_TYPE_RAW_BUFFER = 2,
		DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 4
	}DescriptorTypeFlags;

	using DescriptorType = u32;

	typedef enum ShaderStageFlags {
		SHADER_STAGE_INVALID = 0,
		SHADER_STAGE_VERTEX_SHADER = 1,
		SHADER_STAGE_PIXEL_SHADER = 2
	}ShaderStageFlags;

	using ShaderStage = u32;

	inline VkDescriptorType ToVkDescriptorType(DescriptorType type) {
		switch (type)
		{
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DESCRIPTOR_TYPE_RAW_BUFFER:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		default:
			LOG_ERROR("invalid descriptor type: ", type);
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}

	}

	inline VkShaderStageFlags ToVkShaderStageFlags(ShaderStage stage) {
		VkShaderStageFlags flags = 0;
		if (stage & SHADER_STAGE_VERTEX_SHADER) {
			flags |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		if (stage & SHADER_STAGE_PIXEL_SHADER) {
			flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		if (flags == 0) {
			LOG_ERROR("invalid shader stage: ", stage);
		}
		return flags;
	}

	struct PushConstantRange {
		ShaderStage      stages;
		u32              offset;
		u32              size;
	};

	struct PushConstants {
		std::vector<PushConstantRange> pushConstantRanges;
	};

}