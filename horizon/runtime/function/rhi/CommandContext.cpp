#include "CommandContext.h"

Horizon::RHI::CommandContext::CommandContext() noexcept
{
}

Horizon::RHI::CommandContext::~CommandContext() noexcept
{
}

Horizon::RHI::CommandList::CommandList(CommandQueueType type) noexcept : m_type(type)
{
}

Horizon::RHI::CommandList::~CommandList() noexcept
{
}
