#include <array>

#include "RenderPass.h"

#include <runtime/core/log/Log.h>

namespace Horizon {

	RenderPass::RenderPass(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachment_create_info) :m_device(device)
	{
		CreateRenderPass(attachment_create_info);
	}
	void RenderPass::CreateRenderPass(const std::vector<AttachmentCreateInfo>& attachment_create_info)
	{
		u32 attachmentCount = attachment_create_info.size();
		colorAttachmentCount = attachmentCount;
		std::vector<VkAttachmentDescription> attachmentsDesc(attachmentCount);

		for (u32 i = 0; i < attachmentsDesc.size(); i++) {
			attachmentsDesc[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentsDesc[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentsDesc[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
			attachmentsDesc[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentsDesc[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentsDesc[i].format =ToVkImageFormat(attachment_create_info[i].format);
			attachmentsDesc[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (attachment_create_info[i].usage & AttachmentUsageFlags::PRESENT_SRC) {
				if (attachment_create_info[i].usage & AttachmentUsageFlags::COLOR_ATTACHMENT) {
					attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				}
				else if (attachment_create_info[i].usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT) {
					attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
			}
			else if (attachment_create_info[i].usage & AttachmentUsageFlags::COLOR_ATTACHMENT) {
				attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			else if (attachment_create_info[i].usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT) {
				attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else {
				LOG_ERROR("not an valid attachment");
			}
		}

		// if attachements have depth attachment, it should be the last attachment
		m_has_depth_attachment = false;
		if (attachment_create_info[attachmentCount - 1].usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT) {
			m_has_depth_attachment = true;
			colorAttachmentCount -= 1;
			attachmentsDesc[attachmentCount - 1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}

		
		std::vector<VkAttachmentReference> attachmentReferences(attachmentCount);
		for (u32 i = 0; i < attachmentReferences.size(); i++)
		{
			attachmentReferences[i].attachment = i;
			attachmentReferences[i].layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}
		// set last attachment as depth
		if (m_has_depth_attachment) {
			attachmentReferences[attachmentCount - 1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = m_has_depth_attachment ? attachmentCount - 1 : attachmentCount;
		subpass.pColorAttachments = attachmentReferences.data();
		subpass.pDepthStencilAttachment = m_has_depth_attachment ? &attachmentReferences[attachmentCount - 1] : nullptr;

		//
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT ;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT ;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<u32>(attachmentsDesc.size());
		renderPassInfo.pAttachments = attachmentsDesc.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();

		CHECK_VK_RESULT(vkCreateRenderPass(m_device->Get(), &renderPassInfo, nullptr, &m_render_pass));
	}

	RenderPass::~RenderPass()
	{
		vkDestroyRenderPass(m_device->Get(), m_render_pass, nullptr);
	}

	VkRenderPass RenderPass::Get() const noexcept 
	{
		return m_render_pass;
	}
}