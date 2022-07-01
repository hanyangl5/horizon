#include "ShaderProgram.h"

namespace Horizon::RHI {


	ShaderProgram::ShaderProgram(IDxcBlob* dxil_byte_code) noexcept : shader_byte_code(dxil_byte_code)
	{
		shader_byte_code = dxil_byte_code;
	}

	ShaderProgram::ShaderProgram(VkShaderModule vk_shader_module) noexcept : shader_module(vk_shader_module)
	{

	}

	ShaderProgram::~ShaderProgram() noexcept
	{
	}

	void* ShaderProgram::GetBufferPointer() const noexcept
	{
		return shader_byte_code->GetBufferPointer();
	}

	u64 ShaderProgram::GetBufferSize() const noexcept
	{
		return shader_byte_code->GetBufferSize();
	}
}