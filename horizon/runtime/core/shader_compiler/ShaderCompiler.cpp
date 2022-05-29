#include <fstream>

#include <d3dcompiler.h>

#include "ShaderCompiler.h"
#include <runtime/core/log/Log.h>
#include <runtime/core/utils/definations.h>
#include "dxc/dxcapi.h"
#include <d3d12shader.h>
namespace Horizon {

	ShaderComplier::ShaderComplier()
	{
		InitializeShaderCompiler();
	}

	ShaderComplier::~ShaderComplier()
	{
	}
	void ShaderComplier::InitializeShaderCompiler()
	{

	}

	void Horizon::ShaderComplier::CompileFromSource(
		ShaderTargetPlatform platform,
		ShaderTargetStage stage,
		const std::string& entry_point,
		u32 compile_flags,
		std::vector<char> source_code,
		void** byte_code)
	{
		if (platform == ShaderTargetPlatform::SPIRV) {
			
		}
		else if (platform == ShaderTargetPlatform::DXIL) {
			//IDxcUtils* utils;
			//DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

//			IDxcUtils* pUtils;
//			IDxcCompiler3* pCompiler;
//			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
//			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
//
//			ID3DBlob* errormsg;
//			u32 compileFlags = 0;
//#ifndef NDEBUG
//			compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
//#endif // 
//			CHECK_DX_RESULT(D3DCompile(&source_code[0], source_code.size(), nullptr, nullptr, nullptr, entry_point.c_str(), "vs_5_0", compileFlags, 0, reinterpret_cast<ID3DBlob**>(byte_code), &errormsg));
//
//			if (errormsg) {
//				std::string em{ (char*)errormsg->GetBufferPointer(),errormsg->GetBufferSize() };
//				LOG_ERROR("failed to compile shader: {}", em);
//			}
		}

	}

	std::vector<char> ShaderComplier::read_file(const std::string& path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			LOG_ERROR("failed to open shader file: {}", path);
		}
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}


}