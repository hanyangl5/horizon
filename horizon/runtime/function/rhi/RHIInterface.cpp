#include <runtime/function/rhi/RHIInterface.h>

namespace Horizon::RHI {
RHIInterface::RHIInterface() noexcept {
    m_shader_compiler = std::make_shared<ShaderCompiler>();
}
RHIInterface::~RHIInterface() noexcept {

}
} // namespace Horizon::RHI