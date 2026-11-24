#pragma once

namespace slang
{
struct IGlobalSession;
}

namespace hz
{
// Created once per process, shared by every shader compilation; safe to call from any thread.
slang::IGlobalSession* GetSlangGlobalSession();
} // namespace hz
