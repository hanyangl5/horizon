find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(HORIZON_GPU_DEPS_JSON "${ENGINE_DIR}/Scripts/GpuDeps.json")
set(HORIZON_GPU_DEPS_CMAKE "${CMAKE_BINARY_DIR}/HorizonGpuDeps.cmake")

if(NOT EXISTS "${HORIZON_GPU_DEPS_JSON}")
    message(FATAL_ERROR "GPU dependency manifest is missing: ${HORIZON_GPU_DEPS_JSON}")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c
            "import json, pathlib, sys
deps = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding='utf-8'))['dependencies']
agility = deps['agility_sdk']
directstorage = deps['direct_storage']
print(f'set(HORIZON_AGILITYSDK_VERSION \"{agility[\"version\"]}\" CACHE INTERNAL \"Pinned Microsoft.Direct3D.D3D12 NuGet package version\" FORCE)')
print(f'set(HORIZON_AGILITYSDK_SDK_VERSION \"{agility[\"sdk_version\"]}\" CACHE INTERNAL \"D3D12 Agility SDK exported SDK version\" FORCE)')
print(f'set(HORIZON_DIRECTSTORAGE_VERSION \"{directstorage[\"version\"]}\" CACHE INTERNAL \"Pinned Microsoft.Direct3D.DirectStorage NuGet package version\" FORCE)')"
            "${HORIZON_GPU_DEPS_JSON}"
    OUTPUT_VARIABLE HORIZON_GPU_DEPS_CONTENT
    ERROR_VARIABLE HORIZON_GPU_DEPS_ERROR
    RESULT_VARIABLE HORIZON_GPU_DEPS_RESULT
)

if(NOT HORIZON_GPU_DEPS_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to parse ${HORIZON_GPU_DEPS_JSON}: ${HORIZON_GPU_DEPS_ERROR}")
endif()

file(WRITE "${HORIZON_GPU_DEPS_CMAKE}" "${HORIZON_GPU_DEPS_CONTENT}")
include("${HORIZON_GPU_DEPS_CMAKE}")
