project(DirectStorage)

if(NOT DEFINED HORIZON_DIRECTSTORAGE_VERSION)
    message(FATAL_ERROR "DirectStorage version variable is missing. Include CMake/GpuDeps.cmake before adding DirectStorage.")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/dstorage.h")
    message(FATAL_ERROR
        "DirectStorage ${HORIZON_DIRECTSTORAGE_VERSION} is missing. "
        "Run `python Scripts/UpdateGpuDeps.py --deps direct_storage` from the repository root."
    )
endif()

add_library(DirectStorageSDK INTERFACE)
target_include_directories(DirectStorageSDK INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(HORIZON_DIRECTSTORAGE_RUNTIME_DLLS
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/dstorage.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/dstoragecore.dll"
    CACHE INTERNAL "DirectStorage runtime DLLs"
)
