#pragma once
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#define NOMINMAX

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <string>
#include <filesystem>

#include <iostream>

// gl math lib

#include <glm/ext.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

using mat3 = glm::mat3;
using mat4 = glm::mat4;

#define SHADER_DIR "./shaders/"

//
//std::string getDirPath() {
//	return {};
//}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);



enum DescriptorTypeFlags
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