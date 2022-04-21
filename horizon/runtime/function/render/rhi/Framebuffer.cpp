#include "Framebuffer.h"

#include <runtime/core/log/Log.h>

namespace Horizon {

	Framebuffer::Framebuffer(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachment_create_info, RenderContext& render_context, std::shared_ptr<SwapChain> swap_chain) :m_render_context(render_context), m_device(device)
	{
		createAttachmentsResources(attachment_create_info);
		m_render_pass = std::make_shared<RenderPass>(m_device, attachment_create_info);
		if (swap_chain) {
			createFrameBuffer(m_render_context.width, m_render_context.height, m_render_context.swap_chain_image_count, swap_chain);
		}
		else {
			createFrameBuffer(m_render_context.width, m_render_context.height, 1);
		}
	}

	Framebuffer::~Framebuffer()
	{
		vkDestroySampler(m_device->Get(), m_sampler, nullptr);
		for (auto& attachment : m_frame_buffer_attachments) {
			vkDestroyImage(m_device->Get(), attachment.m_image, nullptr);
			vkDestroyImageView(m_device->Get(), attachment.m_image_view, nullptr);
			vkFreeMemory(m_device->Get(), attachment.m_image_memory, nullptr);
		}
		for (auto& framebuffer : m_framebuffer) {
			vkDestroyFramebuffer(m_device->Get(), framebuffer, nullptr);
		}
	}

	VkFramebuffer Framebuffer::Get() const noexcept 
	{
		return m_framebuffer[0];
	}

	VkFramebuffer Framebuffer::Get(u32 index) const noexcept 
	{
		return m_framebuffer[index];
	}

	VkRenderPass Framebuffer::getRenderPass() const noexcept 
	{
		return m_render_pass->Get();
	}

	std::shared_ptr<AttachmentDescriptor> Framebuffer::getDescriptorImageInfo(u32 attachment_index)
	{
		std::shared_ptr<AttachmentDescriptor> attachmentDescriptor = std::make_shared<AttachmentDescriptor>();
		attachmentDescriptor->imageDescriptorInfo = { m_sampler, m_frame_buffer_attachments[attachment_index].m_image_view ,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		return attachmentDescriptor;
	}

	std::vector<VkImage> Framebuffer::getPresentImages()
	{
		//std::vector<VkImage> images;
		//for (auto& framebuffer : m_framebuffer) {
		//	framebuffer
		//}
		//for (auto& attachment : m_frame_buffer_attachments) {
		//	images.push_back(attachment.m_image);
		//}
		//return ();
		return {};
	}

	u32 Framebuffer::getColorAttachmentCount()
	{
		return m_render_pass->colorAttachmentCount;
	}

	std::vector<VkClearValue> Framebuffer::getClearValues()
	{
		std::vector<VkClearValue> clearValues;
		VkClearValue clearColor;
		clearColor.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		VkClearValue clearDepth;
		clearDepth.depthStencil = { 1.0f, 0 };

		for (u32 i = 0; i < m_render_pass->colorAttachmentCount; i++) {
			clearValues.emplace_back(clearColor);
		}
		if (m_render_pass->m_has_depth_attachment) {
			clearValues.emplace_back(clearDepth);
		}
		return clearValues;
	}

	void Framebuffer::createFrameBuffer(u32 width, u32 height, u32 imag_count, std::shared_ptr<SwapChain> swap_chain)
	{
		m_framebuffer.resize(imag_count);
		for (u32 i = 0; i < imag_count; i++)
		{
			std::vector<VkImageView> attachmentsImageViews{};
			if (swap_chain) {
				attachmentsImageViews.emplace_back(swap_chain->getImageView(i));
			}
			else {
				attachmentsImageViews.resize(m_frame_buffer_attachments.size());
				for (u32 j = 0; j < m_frame_buffer_attachments.size(); j++) {
					attachmentsImageViews[j] = m_frame_buffer_attachments[j].m_image_view;
				}
			}
			VkFramebufferCreateInfo frameBufferCreateInfo{};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.renderPass = m_render_pass->Get();
			frameBufferCreateInfo.attachmentCount = static_cast<u32>(attachmentsImageViews.size());
			frameBufferCreateInfo.pAttachments = attachmentsImageViews.data();
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.layers = 1;
			CHECK_VK_RESULT(vkCreateFramebuffer(m_device->Get(), &frameBufferCreateInfo, nullptr, &m_framebuffer[i]));
		}
	}

	void Framebuffer::createAttachmentsResources(const std::vector<AttachmentCreateInfo>& attachment_create_info)
	{
		for (auto& createInfo : attachment_create_info) {
			m_frame_buffer_attachments.emplace_back(m_device, createInfo);
		}

		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		CHECK_VK_RESULT(vkCreateSampler(m_device->Get(), &sampler, nullptr, &m_sampler));

	}




}