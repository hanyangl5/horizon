#include "CommandList.h"


Horizon::RHI::CommandList::CommandList(CommandQueueType type) noexcept : m_type(type)
{
}

Horizon::RHI::CommandList::~CommandList() noexcept
{
}
