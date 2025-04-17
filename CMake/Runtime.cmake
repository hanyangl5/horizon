# Handle library creation here.

set(ENGINE_RUNTIME_SOURCE_DIR ${ENGINE_SOURCE_DIR}/Runtime)
set(ENGINE_RUNTIME Runtime)

# Do some OS checks, and setup accordingly.
set(WINDOWS OFF)

set(DX12 ON)

# Make our APIs into options
option(EXAMPLES "The Forge examples" OFF)
option(DYNAMIC_LIB "Dynamic Library" OFF)

set(ASSIMP OFF)
set(OZZ OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    message("Windows detected. Generating Windows targets.")
    set(WINDOWS ON)
endif()

# Setup some sane API defaults.
set(API_SELECTED ON)

if(${DX12} MATCHES ON)
    #add_compile_definitions(DIRECT3D12) 
endif()

message("\n")

include(Platform)
include(Core)
include(RHI)
include(Profiler)
include(Resources)
include(Graphics)
include(Application)

source_group(TREE ${CORE_INTERFACE_DIR} PREFIX "Header Files" FILES ${CORE_INTERFACE_FILES})
source_group(TREE ${CORE_SOURCE_DIR} PREFIX "Source Files\\Core" FILES ${CORE_INCLUDE_FILES} ${CORE_SOURCE_FILES})

source_group(TREE ${PLATFORM_INTERFACE_DIR} PREFIX "Header Files" FILES ${PLATFORM_INTERFACE_FILES})
source_group(TREE ${PLATFORM_SOURCE_DIR} PREFIX "Source Files\\Platform" FILES ${PLATFORM_INCLUDE_FILES} ${PLATFORM_SOURCE_FILES})

source_group(TREE ${RHI_INTERFACE_DIR} PREFIX "Header Files" FILES ${RHI_INTERFACE_FILES})
source_group(TREE ${RHI_SOURCE_DIR} PREFIX "Source Files\\RHI" FILES ${RHI_INCLUDE_FILES} ${RHI_SOURCE_FILES})

source_group(TREE ${PROFILER_INTERFACE_DIR} PREFIX "Header Files" FILES ${PROFILER_INTERFACE_FILES})
source_group(TREE ${PROFILER_SOURCE_DIR} PREFIX "Source Files\\Profiler" FILES ${PROFILER_INCLUDE_FILES} ${PROFILER_SOURCE_FILES})

source_group(TREE ${RESOURCES_INTERFACE_DIR} PREFIX "Header Files" FILES ${RESOURCES_INTERFACE_FILES})
source_group(TREE ${RESOURCES_SOURCE_DIR} PREFIX "Source Files\\Resources" FILES ${RESOURCES_INCLUDE_FILES} ${RESOURCES_SOURCE_FILES})

source_group(TREE ${GRAPHICS_INTERFACE_DIR} PREFIX "Header Files" FILES ${GRAPHICS_INTERFACE_FILES})
source_group(TREE ${GRAPHICS_SOURCE_DIR} PREFIX "Source Files\\Graphics" FILES ${GRAPHICS_INCLUDE_FILES} ${GRAPHICS_SOURCE_FILES})

source_group(TREE ${APPLICATION_INTERFACE_DIR} PREFIX "Header Files" FILES ${APPLICATION_INTERFACE_FILES})
source_group(TREE ${APPLICATION_SOURCE_DIR} PREFIX "Source Files\\Application" FILES ${APPLICATION_INCLUDE_FILES} ${APPLICATION_SOURCE_FILES})


set(RUNTIME_INTERFACE_FILES
    ${CORE_INTERFACE_FILES}
    ${PLATFORM_INTERFACE_FILES}
    ${RHI_INTERFACE_FILES}
    ${PROFILER_INTERFACE_FILES}
    ${RESOURCES_INTERFACE_FILES}
    ${GRAPHICS_INTERFACE_FILES}
    ${APPLICATION_INTERFACE_FILES}
)

set(RUNTIME_SOURCE_FILES
    ${CORE_INCLUDE_FILES}
    ${CORE_SOURCE_FILES}
    ${PLATFORM_INCLUDE_FILES}
    ${PLATFORM_SOURCE_FILES}
    ${RHI_INCLUDE_FILES}
    ${RHI_SOURCE_FILES}
    ${PROFILER_INCLUDE_FILES}
    ${PROFILER_SOURCE_FILES}
    ${RESOURCES_INCLUDE_FILES}
    ${RESOURCES_SOURCE_FILES}
    ${GRAPHICS_INCLUDE_FILES}
    ${GRAPHICS_SOURCE_FILES}
    ${APPLICATION_INCLUDE_FILES}
    ${APPLICATION_SOURCE_FILES}
)

if(${DYNAMIC_LIB} MATCHES OFF)
    add_library(${ENGINE_RUNTIME} STATIC
        ${RUNTIME_SOURCE_FILES}
        ${RUNTIME_INTERFACE_FILES}
    )

else()
    add_library(${ENGINE_RUNTIME} SHARED
        ${RUNTIME_SOURCE_FILES}
        ${RUNTIME_INTERFACE_FILES}
    )
endif()

set(RUNTIME_INCLUDE_DIR
    ${ENGINE_SOURCE_DIR}
    ${ENGINE_RUNTIME_SOURCE_DIR}/Platform/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/Core/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/RHI/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/Profiler/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/Resources/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/Graphics/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/Application/Public
    ${ENGINE_RUNTIME_SOURCE_DIR}/Scripting/Public
    ${THIRD_PARTY_INCLUDES}
)

target_include_directories(${ENGINE_RUNTIME} PUBLIC
    ${RUNTIME_INCLUDE_DIR}
)

# https://cmake.org/cmake/help/latest/command/target_precompile_headers.html
target_precompile_headers(${ENGINE_RUNTIME} INTERFACE
    #$<$<COMPILE_LANGUAGE:CXX>:${RUNTIME_INTERFACE_FILES}>
    ${RUNTIME_INTERFACE_FILES}
)

target_link_libraries(${ENGINE_RUNTIME} PUBLIC ${RHI_LIBRARIES} ${THIRD_PARTY_DEPS})

target_link_directories(${ENGINE_RUNTIME} PUBLIC ${RHI_LIBRARY_PATHS})

target_compile_definitions(${ENGINE_RUNTIME} PUBLIC ${RHI_DEFINES})

# unity build
#TODO(hyl5): fix tf_malloc/tf_new
#s et_target_properties(${ENGINE_RUNTIME} PROPERTIES UNITY_BUILD ON)

target_compile_features(${ENGINE_RUNTIME} PRIVATE cxx_std_20)



if (${APPLE_PLATFORM} MATCHES ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++20 -stdlib=libc++ -x objective-c++")
    target_compile_options(${ENGINE_RUNTIME} PRIVATE "-fobjc-arc")
endif()



# Add compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${ENGINE_RUNTIME} PRIVATE -fno-rtti -fno-exceptions

        # -Wall -Wextra -Wshadow -pedantic
        #-Werror 
        # -fms-extensions
        # -Wno-language-extension-token
        # -Wno-switch
        # -Wno-missing-field-initializers
        # -Wno-unused-value
        # -Wno-microsoft-enum-value
        # -Wno-nested-anon-types
        # -Wno-gnu-anonymous-struct
        # -Wno-gnu-zero-variadic-macro-arguments
        # -Wno-keyword-macro
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # SET(CMAKE_CXX_FLAGS "/GR- /EHsc- /MP /permissive /w14640 /W4 /WX /external:anglebrackets /external:W0")
    SET(CMAKE_CXX_FLAGS "/EHsc-")
    target_compile_options(${ENGINE_RUNTIME} PRIVATE /GR- /EHsc- /MP /permissive /w14640 /W3 /WX
        /external:anglebrackets /external:W0
        /wd4100)
    # will produe warning, https://cmake.org/pipermail/cmake/2010-December/041639.html
endif()

set_target_properties(${ENGINE_RUNTIME} PROPERTIES FOLDER "Horizon")