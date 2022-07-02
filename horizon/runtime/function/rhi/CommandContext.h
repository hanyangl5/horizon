#pragma once

#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {

	class CommandContext
	{
	public:
		CommandContext() noexcept;
		CommandContext(const CommandContext& command_list) noexcept = default;
		CommandContext(CommandContext&& command_list) noexcept = default;
		virtual ~CommandContext() noexcept;
		virtual void Reset() noexcept = 0;
	protected:

	private:

	};
}
