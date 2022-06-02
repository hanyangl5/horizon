#pragma once

#include <string>
#include <vector>

#include <dxc/dxcapi.h>
#include <d3d12shader.h>

#include <runtime/core/utils/definations.h>

namespace Horizon
{
	enum class ShaderTargetPlatform
	{
		SPIRV,
		DXIL
	};

	enum class ShaderTargetStage
	{
		vs,
		ps,
		cs
	};

	class ShaderProgram {
	public:
		ShaderProgram(IDxcBlob* dxil_byte_code) noexcept;
		ShaderProgram(VkShaderModule vk_shader_module) noexcept;
		~ShaderProgram() noexcept;
		void* GetBufferPointer() const noexcept;
		u64 GetBufferSize() const noexcept;
	//private:
		IDxcBlob* shader_byte_code;
		VkShaderModule shader_module;
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler() noexcept;
		~ShaderCompiler() noexcept;

		IDxcBlob* CompileFromFile(ShaderTargetPlatform platform,
			ShaderTargetStage stage,
			const std::string& entry_point,
			u32 compile_flags,
			std::string file_name) noexcept;

		static std::vector<char> read_file(const std::string &path) noexcept;

	private:
		void InitializeShaderCompiler() noexcept;
	private:
		IDxcUtils* idxc_utils;;
		IDxcCompiler3* idxc_compiler;
	};

}
