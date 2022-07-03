#include <runtime/function/rhi/CommandList.h>

namespace Horizon::RHI {

CommandList::CommandList(CommandQueueType type) noexcept : m_type(type) {}

CommandList::~CommandList() noexcept {}

} // namespace Horizon::RHI
