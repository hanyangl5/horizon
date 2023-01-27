
 #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
 #include <doctest/doctest.h>

 #include "config.h"

namespace TEST::ShaderCompilationTest {

using namespace Horizon;

class ShaderCompilationTest {
  public:
    ShaderCompilationTest() {}

  public:
};

TEST_CASE_FIXTURE(ShaderCompilationTest, "test") {
    ShaderCompilationArgs args;
    args.entry_point = "CS_MAIN";
    args.optimization_level = ShaderOptimizationLevel::DEBUG;
    args.target_api = ShaderTargetAPI::SPIRV;
    args.target_profile = ShaderTargetProfile::CS_6_6;
    args.output_file_name = "ssao.hsb";
    args.include_path = "C:/hylu/horizon/horizon/assets/hlsl/include";
    std::string path = "C:/hylu/horizon/horizon/assets/hlsl/ssao.comp.hlsl";

    ShaderCompiler::get().CompileFromFile(path.c_str(), args);

    // threading
}
} // namespace TEST::ShaderCompilationTest