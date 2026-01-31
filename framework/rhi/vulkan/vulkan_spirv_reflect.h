#pragma once

#include <rhi/enums.h>

#include <cstddef>
#include <cstdint>

namespace Horizon::Backend {

void ReflectSpirvToRootSignature(const void *spirv, size_t size, ShaderType stage,
                                 RootSignatureDesc &out) noexcept;

} // namespace Horizon::Backend
