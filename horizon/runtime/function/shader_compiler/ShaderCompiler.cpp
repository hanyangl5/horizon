// #include <fstream>

// #include <d3dcompiler.h>

// #include "ShaderCompiler.h"
// #include <runtime/core/log/Log.h>
// #include <runtime/core/utils/Definations.h>

// namespace Horizon {

// ShaderCompiler::ShaderCompiler() noexcept { InitializeShaderCompiler(); }

// ShaderCompiler::~ShaderCompiler() noexcept {}

// void ShaderCompiler::InitializeShaderCompiler() {
//     DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&idxc_utils));
//     DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&idxc_compiler));
// }

// IDxcBlob *ShaderCompiler::CompileFromFile(ShaderTargetPlatform platform, ShaderType type,
//                                           const std::string &entry_point, u32 compile_flags, std::string file_name) {

//     // read source file
//     IDxcBlobEncoding *source_file = nullptr;
//     idxc_utils->LoadFile(std::wstring(file_name.begin(), file_name.end()).c_str(), nullptr, &source_file);

//     DxcBuffer source_buffer;
//     source_buffer.Ptr = source_file->GetBufferPointer();
//     source_buffer.Size = source_file->GetBufferSize();
//     source_buffer.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

//     IDxcIncludeHandler *pIncludeHandler;
//     idxc_utils->CreateDefaultIncludeHandler(&pIncludeHandler);

//     // fill compile args
//     std::vector<LPCWSTR> compile_args;

//     std::string typestr;
//     switch (type) {
//     case Horizon::ShaderType::VERTEX_SHADER:
//         typestr = "vs";
//         break;
//     case Horizon::ShaderType::PIXEL_SHADER:
//         typestr = "ps";
//         break;
//     case Horizon::ShaderType::COMPUTE_SHADER:
//         typestr = "cs";
//         break;
//         // case Horizon::ShaderType::GEOMETRY_SHADER:
//         //     typestr = "gs";
//         break;
//     default:
//         break;
//     }

//     auto tp = "-T " + typestr + "_6_6";
//     auto ep = "-E " + entry_point;

//     std::wstring wtp(tp.begin(), tp.end());
//     std::wstring wep(ep.begin(), ep.end());

//     compile_args.emplace_back(wep.c_str());
//     compile_args.emplace_back(wtp.c_str());

//     if (platform == ShaderTargetPlatform::SPIRV) {
//         compile_args.emplace_back(L"-spirv");
//     }

//     // compile
//     IDxcResult *compile_result;
//     CHECK_DX_RESULT(idxc_compiler->Compile(&source_buffer, compile_args.data(), compile_args.size(), nullptr,
//                                            IID_PPV_ARGS(&compile_result)));
//     IDxcBlobUtf8 *error_msg;
//     compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error_msg), nullptr);

//     if (error_msg && error_msg->GetStringLength() > 0) {
//         LOG_ERROR("failed to compile shader: {}",
//                   std::string{(char *)error_msg->GetBufferPointer(), error_msg->GetBufferSize()});
//         return nullptr;
//     }

//     IDxcBlob *result_code;
//     compile_result->GetResult(&result_code);

//     {
//         error_msg->Release();
//         compile_result->Release();
//         source_file->Release();
//         pIncludeHandler->Release();
//     }

//     return result_code;
// }

// std::vector<char> ShaderCompiler::read_file(const std::string &path) {
//     std::ifstream file(path, std::ios::ate | std::ios::binary);
//     if (!file.is_open()) {
//         LOG_ERROR("failed to open shader file: {}", path);
//     }
//     size_t fileSize = (size_t)file.tellg();
//     std::vector<char> buffer(fileSize);
//     file.seekg(0);
//     file.read(buffer.data(), fileSize);
//     file.close();

//     return buffer;
// }

// } // namespace Horizon