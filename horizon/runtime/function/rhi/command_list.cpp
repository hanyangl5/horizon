#include "command_list.h"

namespace Horizon::Backend {

CommandList::CommandList(CommandQueueType type) noexcept : m_type(type) {}

CommandList::~CommandList() noexcept {}

} // namespace Horizon::Backend