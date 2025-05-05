# RHI

set(RHI_INTERFACE_DIR ${ENGINE_RUNTIME_SOURCE_DIR}/RHI/Public)
set(RHI_SOURCE_DIR ${ENGINE_RUNTIME_SOURCE_DIR}/RHI/Private)

file(GLOB RHI_INTERFACE_FILES ${RHI_INTERFACE_DIR}/RHI/*.h ${RHI_INTERFACE_DIR}/RHI/*.hpp)

file(GLOB_RECURSE DX12_INCLUDE_FILES ${RHI_SOURCE_DIR}/Direct3D12/*.h ${RHI_SOURCE_DIR}/Direct3D12/*.hpp)
file(GLOB_RECURSE DX12_SOURCE_FILES ${RHI_SOURCE_DIR}/Direct3D12/*.cpp ${RHI_SOURCE_DIR}/Direct3D12/*.c)

file(GLOB RHI_INCLUDE_FILES ${RHI_SOURCE_DIR}/*.h ${RHI_SOURCE_DIR}/*.hpp)
file(GLOB RHI_SOURCE_FILES ${RHI_SOURCE_DIR}/*.cpp ${RHI_SOURCE_DIR}/*.c)

if(${DX12} MATCHES ON)
    set(RHI_LIBRARIES ${RHI_LIBRARIES}
        D3D12MemoryAllocator
        DirectXShaderCompiler
        agilitysdk_d3dx12
    )

    set(RHI_LIBRARIES ${RHI_LIBRARIES}
        "d3d12.lib"
    )

    # add_compile_definitions(D3D12_AGILITY_SDK=1)
    # add_compile_definitions(D3D12_AGILITY_SDK_VERSION=615)
    
    set(RHI_INCLUDE_FILES ${RHI_INCLUDE_FILES} ${DX12_INCLUDE_FILES})
    set(RHI_SOURCE_FILES ${RHI_SOURCE_FILES} ${DX12_SOURCE_FILES})
endif()

if(${WINDOWS} MATCHES ON)
    set(RHI_LIBRARIES ${RHI_LIBRARIES}
        WinPixEventRuntime
        AGS
        Nvapi
    )

    set(RHI_LIBRARIES ${RHI_LIBRARIES}
        "Xinput9_1_0.lib"
        "ws2_32.lib"
    )

    set(RHI_DEFINES ${RHI_DEFINES}
        "_WINDOWS"
    )
endif()