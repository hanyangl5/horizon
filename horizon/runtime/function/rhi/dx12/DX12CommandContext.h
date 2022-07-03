#pragma once

#include "stdafx.h"

#include <array>

#include <runtime/function/rhi/CommandContext.h>
#include <runtime/function/rhi/dx12/DX12CommandList.h>
#include <runtime/function/rhi/dx12/DX12Buffer.h>

namespace Horizon::RHI {

	class DX12CommandContext : public CommandContext
	{
	public:
		DX12CommandContext(ID3D12Device* device) noexcept;
		DX12CommandContext(const DX12CommandContext& command_list) noexcept = default;
		DX12CommandContext(DX12CommandContext&& command_list) noexcept = default;
		virtual ~DX12CommandContext() noexcept override;
		DX12CommandList* GetDX12CommandList(CommandQueueType type) noexcept;
		virtual void Reset() noexcept override;
	private:
		ID3D12Device* m_device;
		// each thread has pools to allocate graphics/compute/transfer commandlist
		std::array<ID3D12CommandAllocator*, 3> m_command_pools{};

		std::array<std::vector<DX12CommandList*>, 3> m_command_lists{};
		std::array<u32, 3> m_command_lists_count;

	};
}
