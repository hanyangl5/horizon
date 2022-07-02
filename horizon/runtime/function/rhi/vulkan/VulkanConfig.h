#pragma once

namespace Horizon {
#define USE_VULKAN
#define VULKAN_API_VERSION VK_API_VERSION_1_3

#define PRINT_VMA_LOG 1

#ifndef NDEBUG
#define VMA_HEAVY_ASSERT(expr) assert(expr)
#define VMA_DEDICATED_ALLOCATION 0
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#define VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY 256
#define VMA_USE_STL_SHARED_MUTEX 0
#define VMA_MEMORY_BUDGET 0
#define VMA_STATS_STRING_ENABLED 0
#define VMA_MAPPING_HYSTERESIS_ENABLED 0

#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.3

#define VMA_DEBUG_LOG(format, ...) do { \
		printf(format, __VA_ARGS__); \
		printf("\n"); \
	} while(false)

#endif // !NDEBUG


}