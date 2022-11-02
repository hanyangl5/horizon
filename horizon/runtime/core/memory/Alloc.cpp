#include "Alloc.h"

namespace Horizon::Memory {

std::pmr::memory_resource *global_memory_resource;

// std::pmr::memory_resource *temporal_memory_resource;

void initialize() { global_memory_resource = new GlobalMemoryAllocator(); }

void destroy() { delete global_memory_resource; }

} // namespace Horizon::Memory