set(ENGINE_THIRD_PARTY_SOURCE_DIR ${ENGINE_SOURCE_DIR}/ThirdParty)

set(HORIZON_THIRD_PARTY_DEPS
    RMem
    MeshOptimizer
    BString
    zstd
    lz4
    gainputstatic
    Ozz
    cpu_features
    DirectX-Headers
    DirectX-Guids
    utils
    imgui
    mimalloc-static
)

add_library(WinPixEventRuntime SHARED IMPORTED)
set_property(TARGET WinPixEventRuntime PROPERTY IMPORTED_LOCATION
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/winpixeventruntime/bin/x64/WinPixEventRuntime.dll
)

set_property(TARGET WinPixEventRuntime PROPERTY IMPORTED_IMPLIB
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/winpixeventruntime/bin/x64/WinPixEventRuntime.lib
)

add_library(AGS SHARED IMPORTED)
set_property(TARGET AGS PROPERTY IMPORTED_LOCATION
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ags/ags_lib/lib/amd_ags_x64.dll
)

set_property(TARGET AGS PROPERTY IMPORTED_IMPLIB
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ags/ags_lib/lib/amd_ags_x64.lib
)
target_include_directories(AGS INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ags)

add_library(Nvapi STATIC IMPORTED)
set_property(TARGET Nvapi PROPERTY IMPORTED_LOCATION
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/nvapi/amd64/nvapi64.lib
)
target_include_directories(Nvapi INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/nvapi)

add_library(DirectXShaderCompiler STATIC IMPORTED)
set_property(TARGET DirectXShaderCompiler PROPERTY IMPORTED_LOCATION
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/lib/x64/dxcompiler.lib
)

add_library(D3D12MemoryAllocator INTERFACE)
target_include_directories(D3D12MemoryAllocator INTERFACE
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/D3D12MemoryAllocator
)

message(STATUS "Building third-party libraries from source.")

# set(BASISU_FILES
# ${ENGINE_THIRD_PARTY_SOURCE_DIR}/basis_universal/transcoder/basisu_transcoder.cpp
# )
# add_library(Basisu STATIC ${BASISU_FILES})


# set(IMGUI_FILES
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imconfig.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imgui_demo.cpp
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imgui_draw.cpp
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imgui_internal.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imgui_widgets.cpp
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imgui.cpp
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/imgui.h
# )
# add_library(Imgui STATIC ${IMGUI_FILES})

# file(GLOB LUA_FILES "${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/*.c")

# set(LUA_FILES
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lapi.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lauxlib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lbaselib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lbitlib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lcode.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lcorolib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lctype.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ldblib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ldebug.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ldo.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ldump.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lfunc.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lgc.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/linit.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/liolib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/llex.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lmathlib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lmem.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/loadlib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lobject.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lopcodes.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/loslib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lparser.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lstate.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lstring.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lstrlib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ltable.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ltablib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/ltm.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lundump.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lutf8lib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lvm.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/lzio.c
# )
# set(LUA_FILES
#     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lua/*.c
# )

# add_library(Lua STATIC ${LUA_FILES})

# set(MINIZIP_FILES
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/aes.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/aescrypt.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/aeskey.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/aesopt.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/aestab.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/aestab.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/brg_endian.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/brg_types.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/hmac.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/hmac.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/sha1.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/sha1.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/sha2.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/lib/brg/sha2.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/zip/miniz.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_crypt.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_crypt.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_crypt_brg.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_os.cpp
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_os.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_strm.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_strm_raw.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_strm_wzaes.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_strm_wzaes.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_strm_zlib.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_strm_zlib.h
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_zip.c
#      ${ENGINE_THIRD_PARTY_SOURCE_DIR}/minizip/mz_zip.h
# )
# add_library(MiniZip STATIC ${MINIZIP_FILES})

set(RMEM_FILES
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/rmem/src/rmem_get_module_info.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/rmem/src/rmem_hook.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/rmem/src/rmem_lib.cpp
)
add_library(RMem STATIC ${RMEM_FILES})

target_include_directories(RMem PRIVATE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/rmem/inc)

set(MESHOPTIMIZER_FILES
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/vertexfilter.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/allocator.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/clusterizer.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/indexcodec.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/indexgenerator.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/meshoptimizer.h
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/overdrawanalyzer.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/overdrawoptimizer.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/simplifier.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/spatialorder.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/stripifier.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/vcacheanalyzer.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/vcacheoptimizer.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/vertexcodec.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/vfetchanalyzer.cpp
     ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src/vfetchoptimizer.cpp
)
add_library(MeshOptimizer STATIC ${MESHOPTIMIZER_FILES})

file(GLOB_RECURSE BSTRING_FILES ${ENGINE_THIRD_PARTY_SOURCE_DIR}/bstrlib/bstrlib.c)
add_library(BString STATIC ${BSTRING_FILES})

file(GLOB_RECURSE ZSTD_FILES ${ENGINE_THIRD_PARTY_SOURCE_DIR}/zstd/*.c)
add_library(zstd STATIC ${ZSTD_FILES})

file(GLOB_RECURSE LZ4_FILES ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lz4/*.c)
add_library(lz4 STATIC ${LZ4_FILES})

file(GLOB_RECURSE IMGUI_FILES ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/*.cpp)
add_library(imgui STATIC ${IMGUI_FILES})
add_subdirectory(${ENGINE_THIRD_PARTY_SOURCE_DIR}/gainput)

    # set(CPU_FEATURES_FILES
    # ${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/src/impl_x86_macos.c
    # ${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/src/impl_aarch64_iOS.c
    # )

    # add_library(cpu_features STATIC ${CPU_FEATURES_FILES})
    add_subdirectory(${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/)
    set(OZZ_INCLUDES
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include
    )
    set(OZZ_BASE_FILES
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/containers/map.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/containers/set.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/containers/string.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/containers/string_archive.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/containers/vector.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/containers/vector_archive.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/endianness.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/gtest_helper.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/io/archive.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/io/archive_traits.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/maths/gtest_math_helper.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/maths/math_archive.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/maths/math_constant.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/maths/math_ex.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/maths/simd_math_archive.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/maths/soa_math_archive.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/memory/allocator.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/base/platform.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/containers/string_archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/io/archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/maths/math_archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/maths/simd_math_archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/maths/soa_math_archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/memory/allocator.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/base/platform.cc
    )
    source_group(Base FILES ${OZZ_BASE_FILES})
    set(OZZ_ANIMATION_FILES
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/ik_aim_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/ik_two_bone_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/animation.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/blending_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/local_to_model_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/sampling_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/skeleton.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/skeleton_utils.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/track.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/track_sampling_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/track_triggering_job.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/runtime/track_triggering_job_trait.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/animation_keyframe.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/animation.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/blending_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/ik_aim_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/ik_two_bone_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/local_to_model_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/sampling_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/skeleton_utils.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/skeleton.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/track_sampling_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/track_triggering_job.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/runtime/track.cc
    )
    source_group(Animation/runtime FILES ${OZZ_ANIMATION_FILES})
    set(OZZ_ANIMATION_OFFLINE_FILES
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/additive_animation_builder.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/animation_builder.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/animation_optimizer.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/raw_animation_utils.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/raw_animation.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/raw_skeleton.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/raw_track.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/skeleton_builder.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/track_builder.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include/ozz/animation/offline/track_optimizer.h
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/additive_animation_builder.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/animation_builder.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/animation_optimizer.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/raw_animation_archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/raw_animation_utils.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/raw_animation.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/raw_skeleton_archive.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/raw_skeleton.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/raw_track.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/skeleton_builder.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/track_builder.cc
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/src/animation/offline/track_optimizer.cc
    )
    source_group(Animation/Offline FILES ${OZZ_ANIMATION_OFFLINE_FILES})
    set(OZZ_FILES
    ${OZZ_BASE_FILES}
    ${OZZ_ANIMATION_FILES}
    ${OZZ_ANIMATION_OFFLINE_FILES}
    )
    add_library(Ozz STATIC ${OZZ_FILES})
    target_include_directories(Ozz PUBLIC ${OZZ_INCLUDES})

    add_subdirectory(${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectX-Headers)
    add_subdirectory(${ENGINE_THIRD_PARTY_SOURCE_DIR}/mimalloc)
    add_subdirectory(${ENGINE_THIRD_PARTY_SOURCE_DIR}/AgilitySDK)
    set(THIRD_PARTY_INCLUDES
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/mimalloc/include
    )

set(THIRD_PARTY_DEPS ${HORIZON_THIRD_PARTY_DEPS})

if(HORIZON_ENABLE_TRACY)
    if(NOT EXISTS ${ENGINE_THIRD_PARTY_SOURCE_DIR}/tracy/public/TracyClient.cpp)
        message(FATAL_ERROR "Tracy submodule is missing. Run `git submodule update --init --recursive` or checkout with submodules enabled.")
    endif()

    add_library(TracyClient STATIC
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/tracy/public/TracyClient.cpp
    )

    target_include_directories(TracyClient PUBLIC
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/tracy/public
    )

    target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE)
    list(APPEND THIRD_PARTY_DEPS TracyClient)
endif()

foreach(LIB ${THIRD_PARTY_DEPS})
    set_target_properties(${LIB} PROPERTIES FOLDER "Horizon/ThirdParty")
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${LIB} PRIVATE /MP)
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${LIB} PRIVATE -Wno-everything)
    endif()
    target_include_directories(${LIB} PRIVATE
        ${ENGINE_SOURCE_DIR}/Runtime/Core/Public
        ${ENGINE_SOURCE_DIR}/Runtime/Platform/Public
        ${ENGINE_SOURCE_DIR}
    )
    target_compile_features(${LIB} PRIVATE cxx_std_20)
endforeach()
