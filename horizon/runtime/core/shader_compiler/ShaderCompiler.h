#pragma once

#include <string>
#include <vector>

#include <runtime/core/utils/definations.h>


namespace Horizon {
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

	class ShaderComplier
	{
	public:
		ShaderComplier();
		~ShaderComplier();
		void InitializeShaderCompiler();
		//void CompileFromFile(ShaderTargetPlatform platform, const std::string& file, void* bytecode);
		void CompileFromSource(
			ShaderTargetPlatform platform,
			ShaderTargetStage stage,
			const std::string& entry_point,
			u32 compile_flags,
			std::vector<char> source_code,
			void** byte_code);
		static std::vector<char> read_file(const std::string& path);
		
	private:

		
	};

}
