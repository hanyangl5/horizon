#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/ResourceCache.h>

namespace Horizon::RHI {

thread_local std::unique_ptr<CommandContext> thread_command_context;

RHIInterface::RHIInterface() noexcept {
    m_shader_compiler = std::make_shared<ShaderCompiler>();
}
RHIInterface::~RHIInterface() noexcept {}

} // namespace Horizon::RHI