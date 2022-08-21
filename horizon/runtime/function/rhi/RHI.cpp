#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/ResourceCache.h>

namespace Horizon::RHI {

thread_local std::unique_ptr<CommandContext> thread_command_context;

RHI::RHI() noexcept { m_shader_compiler = std::make_shared<ShaderCompiler>(); }

RHI::~RHI() noexcept {}

std::vector<char> RHI::ReadFile(const std::string &path) const {
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
} // namespace Horizon::RHI