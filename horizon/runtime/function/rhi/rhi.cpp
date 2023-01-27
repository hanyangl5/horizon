#include "rhi.h"

namespace Horizon::Backend {

thread_local CommandContext *thread_command_context;

RHI::RHI() noexcept {}

RHI::~RHI() noexcept {}

} // namespace Horizon::Backend