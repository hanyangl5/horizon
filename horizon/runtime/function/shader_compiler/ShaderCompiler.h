#pragma once

#include <string>
#include <vector>

#include <dxc/dxcapi.h>
#include <d3d12shader.h>

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/ShaderProgram.h>
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
