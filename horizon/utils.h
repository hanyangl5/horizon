#pragma once

#define NOMINMAX
// stl

#include <string>
#include <filesystem>
#include <iostream>

// thirdparty lib

#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include "Math.h"


namespace Horizon {

	// *******************************
	// HANDLING A VULKAN ERROR RETURN:
	// *******************************

	struct errorcode
	{
		VkResult	resultCode;
		std::string	meaning;
	};

	errorcode ErrorCodes[];

	enum logLevel
	{
		debug,
		info,
		error
	};

	void printVkError(VkResult result, std::string prefix = "", logLevel level = logLevel::info);


	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using u64 = uint64_t;
	using i8 = int8_t;
	using i16 = int16_t;
	using i32 = int32_t;
	using i64 = int64_t;

	using f32 = float;
	using f64 = double;


	uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

	enum class DescriptorTypeFlags
	{
		UNIFORM_BUFFER,
		STORAGE_BUFFER,
		COMBINDED_SAMPLER
	};

	using DescriptorType = u32;

	struct DescriptorBase
	{
		//DescriptorType type;
		VkDescriptorImageInfo imageDescriptorInfo{};
		VkDescriptorBufferInfo bufferDescriptrInfo{};
	};

}