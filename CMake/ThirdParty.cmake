set(ENGINE_THIRD_PARTY_SOURCE_DIR ${ENGINE_SOURCE_DIR}/ThirdParty)

option(HORIZON_REBUILD_THIRDPARTY "Build third-party libraries from source instead of using checked-in prebuilts." OFF)
set(HORIZON_THIRD_PARTY_PREBUILT_ROOT "${ENGINE_THIRD_PARTY_SOURCE_DIR}/Prebuilt" CACHE PATH "Root directory for checked-in third-party binary libraries.")
set(HORIZON_THIRD_PARTY_PREBUILT_PLATFORM "" CACHE STRING "Third-party prebuilt platform folder. Empty means auto-detect.")
set(HORIZON_THIRD_PARTY_PREBUILT_CONFIGS "Debug;Release" CACHE STRING "Configuration folders required under the selected prebuilt platform.")

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

function(horizon_get_third_party_prebuilt_platform output_variable)
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" system_name)
    if(system_name STREQUAL "windows")
        set(system_name "windows")
    elseif(system_name STREQUAL "darwin")
        set(system_name "macos")
    elseif(system_name STREQUAL "linux")
        set(system_name "linux")
    endif()

    if(MSVC)
        set(compiler_name "msvc")
    else()
        string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" compiler_name)
    endif()

    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" arch_name)
    if(arch_name MATCHES "^(amd64|x86_64|x64)$")
        set(arch_name "x64")
    elseif(arch_name MATCHES "^(aarch64|arm64)$")
        set(arch_name "arm64")
    elseif(arch_name MATCHES "^(x86|i[3-6]86)$")
        set(arch_name "x86")
    endif()

    set(${output_variable} "${system_name}-${compiler_name}-${arch_name}" PARENT_SCOPE)
endfunction()

horizon_get_third_party_prebuilt_platform(HORIZON_THIRD_PARTY_DEFAULT_PREBUILT_PLATFORM)
if(NOT HORIZON_THIRD_PARTY_PREBUILT_PLATFORM)
    set(HORIZON_THIRD_PARTY_PREBUILT_PLATFORM "${HORIZON_THIRD_PARTY_DEFAULT_PREBUILT_PLATFORM}" CACHE STRING "Third-party prebuilt platform folder. Empty means auto-detect." FORCE)
endif()
set(HORIZON_THIRD_PARTY_PREBUILT_DIR "${HORIZON_THIRD_PARTY_PREBUILT_ROOT}/${HORIZON_THIRD_PARTY_PREBUILT_PLATFORM}")

function(horizon_import_static_third_party target_name library_file_name)
    add_library(${target_name} STATIC IMPORTED GLOBAL)

    set(imported_configs)
    set(missing_libraries)
    foreach(config_name IN LISTS HORIZON_THIRD_PARTY_PREBUILT_CONFIGS)
        string(TOUPPER "${config_name}" config_name_upper)
        list(APPEND imported_configs "${config_name_upper}")
        set(library_path "${HORIZON_THIRD_PARTY_PREBUILT_DIR}/${config_name}/lib/${library_file_name}")
        set_property(TARGET ${target_name} PROPERTY IMPORTED_LOCATION_${config_name_upper} "${library_path}")

        if(NOT EXISTS "${library_path}")
            list(APPEND missing_libraries "${library_path}")
        endif()
    endforeach()

    set_property(TARGET ${target_name} PROPERTY IMPORTED_CONFIGURATIONS "${imported_configs}")

    list(FIND imported_configs "RELEASE" has_release_config)
    if(NOT has_release_config EQUAL -1)
        set_property(TARGET ${target_name} PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)
        set_property(TARGET ${target_name} PROPERTY MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE)
    endif()

    if(missing_libraries)
        string(REPLACE ";" "\n  " missing_libraries_text "${missing_libraries}")
        message(FATAL_ERROR
            "Missing prebuilt third-party library for ${target_name}.\n"
            "Expected:\n  ${missing_libraries_text}\n"
            "Build from source with -DHORIZON_REBUILD_THIRDPARTY=ON, then run the HorizonPackageThirdParty target.")
    endif()
endfunction()

function(horizon_configure_prebuilt_third_party)
    message(STATUS "Using prebuilt third-party libraries from ${HORIZON_THIRD_PARTY_PREBUILT_DIR}")

    horizon_import_static_third_party(RMem "RMem${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(RMem INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/rmem/inc)

    horizon_import_static_third_party(MeshOptimizer "MeshOptimizer${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(MeshOptimizer INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/meshoptimizer/src)

    horizon_import_static_third_party(BString "BString${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(BString INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/bstrlib)

    horizon_import_static_third_party(zstd "zstd${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(zstd INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/zstd)

    horizon_import_static_third_party(lz4 "lz4${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(lz4 INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/lz4)

    horizon_import_static_third_party(gainputstatic "gainputstatic${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(gainputstatic INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/gainput/lib/include)
    if(WIN32)
        target_link_libraries(gainputstatic INTERFACE ws2_32)
    endif()

    horizon_import_static_third_party(Ozz "Ozz${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(Ozz INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/ozz-animation/include)

    horizon_import_static_third_party(cpu_features "cpu_features${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(cpu_features INTERFACE
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/include
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/include/internal
    )
    target_compile_definitions(cpu_features INTERFACE STACK_LINE_READER_BUFFER_SIZE=1024)
    target_link_libraries(cpu_features INTERFACE ${CMAKE_DL_LIBS})
    add_library(CpuFeatures::cpu_features ALIAS cpu_features)

    horizon_import_static_third_party(DirectX-Headers "DirectX-Headers${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(DirectX-Headers INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectX-Headers/include)
    if(NOT WIN32)
        target_include_directories(DirectX-Headers INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectX-Headers/include/wsl/stubs)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(DirectX-Headers INTERFACE "-D__REQUIRED_RPCNDR_H_VERSION__=475")
    endif()
    add_library(Microsoft::DirectX-Headers ALIAS DirectX-Headers)

    horizon_import_static_third_party(DirectX-Guids "DirectX-Guids${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_link_libraries(DirectX-Guids INTERFACE DirectX-Headers)
    add_library(Microsoft::DirectX-Guids ALIAS DirectX-Guids)

    add_library(utils INTERFACE IMPORTED GLOBAL)
    target_include_directories(utils INTERFACE
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/include
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/cpu_features/include/internal
    )
    target_compile_definitions(utils INTERFACE STACK_LINE_READER_BUFFER_SIZE=1024)

    horizon_import_static_third_party(imgui "imgui${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(imgui INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui)

    horizon_import_static_third_party(mimalloc-static "mimalloc-static${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(mimalloc-static INTERFACE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/mimalloc/include)

    set(THIRD_PARTY_INCLUDES
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/mimalloc/include
        PARENT_SCOPE
    )
    set(THIRD_PARTY_DEPS ${HORIZON_THIRD_PARTY_DEPS} PARENT_SCOPE)

    foreach(third_party_target IN LISTS HORIZON_THIRD_PARTY_DEPS)
        if(TARGET ${third_party_target})
            set_target_properties(${third_party_target} PROPERTIES FOLDER "Horizon/ThirdParty")
        endif()
    endforeach()
endfunction()

function(horizon_add_third_party_package_target)
    set(package_targets
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
        imgui
        mimalloc-static
    )

    add_custom_target(HorizonPackageThirdParty DEPENDS ${package_targets})
    set_target_properties(HorizonPackageThirdParty PROPERTIES FOLDER "Horizon/ThirdParty")

    foreach(package_target IN LISTS package_targets)
        add_custom_command(TARGET HorizonPackageThirdParty POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${HORIZON_THIRD_PARTY_PREBUILT_DIR}/$<CONFIG>/lib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${package_target}>" "${HORIZON_THIRD_PARTY_PREBUILT_DIR}/$<CONFIG>/lib/$<TARGET_FILE_NAME:${package_target}>"
            VERBATIM
        )
    endforeach()
endfunction()

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

if(HORIZON_REBUILD_THIRDPARTY)
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

horizon_add_third_party_package_target()

else()
    horizon_configure_prebuilt_third_party()
endif()
