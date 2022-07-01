#pragma once

#include <runtime/function/rhi/RHIUtils.h>
#include <dxc/dxcapi.h>
#include <d3d12shader.h>
namespace Horizon::RHI {	
	class ShaderProgram {
	public:
		ShaderProgram(IDxcBlob* dxil_byte_code) noexcept;
		ShaderProgram(VkShaderModule vk_shader_module) noexcept;
		~ShaderProgram() noexcept;
		void* GetBufferPointer() const noexcept;
		u64 GetBufferSize() const noexcept;
		ShaderType GetType() const noexcept {
			return m_type;
		}
	private:
		ShaderType m_type;
	//private:
		IDxcBlob* shader_byte_code;
		VkShaderModule shader_module;
	};

}