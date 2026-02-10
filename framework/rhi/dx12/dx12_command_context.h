#pragma once

#include "dx12_command_allocator.h"
#include "dx12_utils.h"
#include <d3d12.h>

#include <core/definations.h>
#include <rhi/command_context.h>

namespace Horizon::Backend
{

class DX12CommandList;

class DX12CommandContext : public CommandContext
{
  public:
    DX12CommandContext(const DX12RendererContext &context) noexcept;
    virtual ~DX12CommandContext() noexcept override;

    DX12CommandContext(const DX12CommandContext &command_list) noexcept = delete;
    DX12CommandContext(DX12CommandContext &&command_list) noexcept = delete;
    DX12CommandContext &operator=(const DX12CommandContext &rhs) noexcept = delete;
    DX12CommandContext &operator=(DX12CommandContext &&rhs) noexcept = delete;

    virtual CommandList *GetCommandList(CommandQueueType type) override;
    virtual void Reset() override;

  private:
    const DX12RendererContext &m_context;
    std::array<std::unique_ptr<DX12CommandAllocatorPool>, 3> m_allocator_pools;
    std::array<std::vector<DX12CommandList *>, 3> m_command_lists;
};

} // namespace Horizon::Backend
