#include "shader_compiler.h"

#include <runtime/core/log/log.h>
#include <runtime/core/memory/allocators.h>
#include <runtime/core/platform/platform.h>
#include <runtime/core/utils/functions.h>

namespace Horizon {

ShaderCompiler::ShaderCompiler() noexcept {
    CHECK_DX_RESULT(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&idxc_compiler)));
    CHECK_DX_RESULT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&idxc_utils)));
    CHECK_DX_RESULT(idxc_utils->CreateDefaultIncludeHandler((&idxc_include_handler)));
}

ShaderCompiler::~ShaderCompiler() noexcept {}

void ShaderCompiler::CompileFromFile(const std::filesystem::path &path, const ShaderCompilationArgs &compile_args) {
    this->Compile(ReadFile(path.string().c_str()), compile_args);
}

void ShaderCompiler::Compile(const Container::Array<char> &hlsl_text, const ShaderCompilationArgs &compile_args) {
    IDxcBlobEncoding *hlsl_blob{};
    CHECK_DX_RESULT(idxc_utils->CreateBlob(hlsl_text.data(), static_cast<u32>(hlsl_text.size()), 0, &hlsl_blob));

    auto stack_memory = Memory::GetStackMemoryResource(512);
    Container::Array<LPCWSTR> compilation_arguments(&stack_memory);

    // entry point
    Container::WString ep(&stack_memory);
    ep = {compile_args.entry_point.begin(), compile_args.entry_point.end()};
    compilation_arguments.push_back(L"-E");
    compilation_arguments.push_back(ep.c_str());
    // target profile
    compilation_arguments.push_back(L"-T");
    const wchar_t* tp = ToDxcTargetProfile(compile_args.target_profile);
    compilation_arguments.push_back(tp);

    compilation_arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); // warning are errors
    compilation_arguments.push_back(DXC_ARG_ALL_RESOURCES_BOUND);
    compilation_arguments.push_back(DXC_ARG_ALL_RESOURCES_BOUND);
    //compilation_arguments.push_back(L"Fre");
    //Container::WString ref_file =
    //    Container::WString{compile_args.output_file_name.begin(), compile_args.output_file_name.end()} + L".ref";

    //compilation_arguments.push_back(ref_file.c_str());

    Container::WString ip(&stack_memory);
    ip = {compile_args.include_path.begin(), compile_args.include_path.end()};
    compilation_arguments.push_back(L"I");
    compilation_arguments.push_back(ip.c_str());
    
    // descriptor set definations
    if (0) {
        compilation_arguments.push_back(L"-D");
        compilation_arguments.push_back(L"UPDATE_FREQ_NONE 0");
        compilation_arguments.push_back(L"-D");
        compilation_arguments.push_back(L"UPDATE_FREQ_PER_FRAME 1");
        compilation_arguments.push_back(L"-D");
        compilation_arguments.push_back(L"UPDATE_FREQ_PER_MATERIAL 2");
        compilation_arguments.push_back(L"-D");
        compilation_arguments.push_back(L"UPDATE_FREQ_PER_DRAW 3");
        compilation_arguments.push_back(L"-D");
        compilation_arguments.push_back(L"UPDATE_FREQ_BINDLESS 4");
    }

    if (USE_ROW_MAJOR_MATRIX) {
        compilation_arguments.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);
    }

    if (compile_args.optimization_level == ShaderOptimizationLevel::DEBUG) {
        compilation_arguments.push_back(DXC_ARG_DEBUG);

        //TODO(hylu): debug
        //compilation_arguments.push_back(L"-Fo");
        //Container::WString pdb_name = out_file_name / ".hlslpdb";
        //compilation_arguments.push_back(pdb_name.c_str());
    } else if (compile_args.optimization_level == ShaderOptimizationLevel::O3) {
        compilation_arguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
    }

    if (compile_args.target_api == ShaderTargetAPI::SPIRV) {
        compilation_arguments.push_back(L"-spirv");
    }

    DxcBuffer source_buffer{hlsl_blob->GetBufferPointer(), hlsl_blob->GetBufferSize(), 0u};

    IDxcResult *compile_result{};
    CHECK_DX_RESULT(idxc_compiler->Compile(&source_buffer, compilation_arguments.data(),
                                           static_cast<u32>(compilation_arguments.size()), idxc_include_handler,
                                           IID_PPV_ARGS(&compile_result)));

    // handle errors

    // Get compilation errors (if any).
    IDxcBlobUtf8 *errors{};
    CHECK_DX_RESULT(compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
    if (errors && errors->GetStringLength() > 0) {
        const LPCSTR errorMessage = errors->GetStringPointer();
        LOG_ERROR("shader compilation failed {}", errorMessage);
    }

    // save result to disk
    // TODO(hylu): prevent expose std::iostream to handle IO
    std::ofstream os;
    os.open(compile_args.output_file_name, std::ios::binary | std::ios::out);
    struct ShaderBlobHeader {
        //
        // version
        // magic number
        u32 size;
        u32 reflection_blob;
        u32 dxil_offset;
        u32 spirv_offset;
    } header{};
    os.write(reinterpret_cast<const char *>(&header), sizeof(ShaderBlobHeader));

    // IR/IL
    //TODO(hylu) save both dxil and spirv, for changing backend in runtime?
    if (compile_args.target_api == ShaderTargetAPI::DXIL) {

    } else if (compile_args.target_api == ShaderTargetAPI::SPIRV) {
        IDxcBlob *spirv_code;
        compile_result->GetResult(&spirv_code);
        os.write(static_cast<const char *>(spirv_code->GetBufferPointer()), spirv_code->GetBufferSize());
    }

    // create reflection data

    os.close();
    //IDxcBlob *reflection_blob{};
    //CHECK_DX_RESULT(compile_result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflection_blob), nullptr));

    //DxcBuffer reflectionBuffer{reflection_blob->GetBufferPointer(), reflection_blob->GetBufferSize(), 0};

    //ID3D12ShaderReflection *shaderReflection{};
    //idxc_utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
    //D3D12_SHADER_DESC shaderDesc{};
    //shaderReflection->GetDesc(&shaderDesc);

    // release resources
}

} // namespace Horizon
