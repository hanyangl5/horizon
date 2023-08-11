#pragma once

namespace Horizon {
#define USE_VULKAN
#define VULKAN_API_VERSION VK_API_VERSION_1_3

#define PRINT_VMA_LOG 1

//#ifndef NDEBUG

//#define VMA_DEBUG_LOG(format, ...)                                             \
//    do {                                                                       \
//        printf(format, __VA_ARGS__);                                           \
//        printf("\n");                                                          \
//    } while (false)
//
//#endif // !NDEBUG

} // namespace Horizon