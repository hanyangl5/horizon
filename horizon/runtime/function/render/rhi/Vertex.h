#pragma once

#include <vulkan/vulkan.hpp>

#include <runtime/function/render/RenderContext.h>

namespace Horizon {
	struct Vertex
	{
		Math::vec3 pos;
		Math::vec3 normal;
		Math::vec2 uv0;
		//vec3 tangent;
		//vec3 bitangent;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
			return bindingDescription;
		}

		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions{
			{0, 0, VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex, pos)},
			{1, 0, VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex, normal)},
			{2, 0, VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex, uv0)},
			//{3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex, tangent)},
			//{4,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex, bitangent)},
			};

			return attributeDescriptions;
		}
	};

}