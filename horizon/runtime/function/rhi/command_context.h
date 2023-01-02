#pragma once

#include <runtime/function/rhi/command_list.h>
#include <runtime/function/rhi/rhi_utils.h>

namespace Horizon::Backend {

class CommandContext {
  public:
    CommandContext() noexcept;
    virtual ~CommandContext() noexcept;
    CommandContext(const CommandContext &rhs) noexcept = delete;
    CommandContext &operator=(const CommandContext &rhs) noexcept = delete;
    CommandContext(CommandContext &&rhs) noexcept = delete;
    CommandContext &operator=(CommandContext &&rhs) noexcept = delete;

    virtual CommandList *GetCommandList(CommandQueueType type) = 0;
    virtual void Reset() = 0;

  protected:
  private:
};
} // namespace Horizon::Backend
