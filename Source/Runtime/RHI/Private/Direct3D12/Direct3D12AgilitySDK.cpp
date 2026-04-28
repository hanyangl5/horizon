#include <stdint.h>

#if defined(_WIN32) && defined(D3D12_AGILITY_SDK)
extern "C"
{
    __declspec(dllexport) extern const uint32_t D3D12SDKVersion = D3D12_AGILITY_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}
#endif
