#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/ResourceCache.h>

namespace Horizon::RHI {

thread_local std::unique_ptr<CommandContext> thread_command_context;

RHI::RHI() noexcept { m_shader_compiler = std::make_shared<ShaderCompiler>(); }
RHI::~RHI() noexcept {}

} // namespace Horizon::RHI