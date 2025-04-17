# Core Files

set(CORE_INTERFACE_DIR ${ENGINE_RUNTIME_SOURCE_DIR}/Core/Public)
set(CORE_SOURCE_DIR ${ENGINE_RUNTIME_SOURCE_DIR}/Core/Private)

# set(CORE_ANDROID_FILES
# ${CORE_SOURCE_DIR}/Android/*.c
# ${CORE_SOURCE_DIR}/Android/*.cpp
# )

# set(CORE_CAMERA_FILES
# ${CORE_SOURCE_DIR}/Camera/CameraController.cpp
# )
file(GLOB_RECURSE CORE_INTERFACE_FILES ${CORE_INTERFACE_DIR}/Core/*.h ${CORE_INTERFACE_DIR}/Core/*.hpp)
file(GLOB_RECURSE CORE_INCLUDE_FILES ${CORE_SOURCE_DIR}/*.h PREFIX "Header" ${CORE_SOURCE_DIR}/*.hpp)
file(GLOB_RECURSE CORE_SOURCE_FILES ${CORE_SOURCE_DIR}/*.cpp "Source" ${CORE_SOURCE_DIR}/**.c)

# source_group(TREE "${ENGINE_RUNTIME_SOURCE_DIR}" FILES ${CORE_INCLUDE_FILES} ${CORE_SOURCE_FILES})
# message(${CORE_INCLUDE_FILES}, ${CORE_SOURCE_FILES})
# set(CORE_CORE_FILES
#     ${CORE_SOURCE_DIR}/Threading/Atomics.h
#     ${CORE_SOURCE_DIR}/GPUConfig.h
#     ${CORE_SOURCE_DIR}/RingBuffer.h
#     ${CORE_SOURCE_DIR}/Config.h
#     ${CORE_SOURCE_DIR}/DLL.h
#     ${CORE_SOURCE_DIR}/RingBuffer.h
#     ${CORE_SOURCE_DIR}/Screenshot.cpp
#     ${CORE_SOURCE_DIR}/TextureContainers.h
#     ${CORE_SOURCE_DIR}/ThreadSystem.cpp
#     ${CORE_SOURCE_DIR}/ThreadSystem.h
#     ${CORE_SOURCE_DIR}/Timer.c
#     ${CORE_SOURCE_DIR}/UnixThreadID.h
#     ${CORE_SOURCE_DIR}/CPUConfig.cpp
# )

# set(CORE_FILESYSTEM_FILES
#     ${CORE_SOURCE_DIR}/FileSystem/FileSystem.cpp
#     ${CORE_SOURCE_DIR}/FileSystem/SystemRun.cpp
#     ${CORE_SOURCE_DIR}/FileSystem/ZipFileSystem.c
# )

# # set(CORE_FONT_FILES
# #     ${CORE_SOURCE_DIR}/Fonts/FontSystem.cpp
# #     ${CORE_SOURCE_DIR}/Fonts/stbtt.cpp
# # )

# # set(CORE_FONT_SHADER_FILES
# #     ${CORE_SOURCE_DIR}/Fonts/Shaders/FSL/fontstash.frag.fsl
# #     ${CORE_SOURCE_DIR}/Fonts/Shaders/FSL/fontstash2D.vert.fsl
# #     ${CORE_SOURCE_DIR}/Fonts/Shaders/FSL/fontstash3D.vert.fsl
# #     ${CORE_SOURCE_DIR}/Fonts/Shaders/FSL/resources.h
# # )

# # set(CORE_INPUT_FILES
# #     ${CORE_SOURCE_DIR}/Input/InputSystem.cpp
# # )

# # set(CORE_INTERFACES_FILES
# #     ${CORE_SOURCE_DIR}/Interfaces/IApp.h
# #     ${CORE_SOURCE_DIR}/Interfaces/ICameraController.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IFileSystem.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IFont.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IInput.h
# #     ${CORE_SOURCE_DIR}/Interfaces/ILog.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IMemory.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IMiddleware.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IOperatingSystem.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IProfiler.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IScreenshot.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IScripting.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IThread.h
# #     ${CORE_SOURCE_DIR}/Interfaces/ITime.h
# #     ${CORE_SOURCE_DIR}/Interfaces/IUI.h
# # )

# set(CORE_LOGGING_FILES
#     ${CORE_SOURCE_DIR}/Logging/Log.c
#     ${CORE_SOURCE_DIR}/Logging/Log.h
# )

# set(CORE_MATH_FILES
#     ${CORE_SOURCE_DIR}/Math/MathTypes.h
#     ${CORE_SOURCE_DIR}/Math/RTree.h
# )

# set(CORE_MEMORYTRACKING_FILES
#     ${CORE_SOURCE_DIR}/MemoryTracking/MemoryTracking.c
#     ${CORE_SOURCE_DIR}/MemoryTracking/NoMemoryDefines.h
# )

# # set(CORE_PROFILER_FILES
# #     ${CORE_SOURCE_DIR}/Profiler/GpuProfiler.cpp
# #     ${CORE_SOURCE_DIR}/Profiler/GpuProfiler.h
# #     ${CORE_SOURCE_DIR}/Profiler/ProfilerBase.cpp
# #     ${CORE_SOURCE_DIR}/Profiler/ProfilerBase.h
# #     ${CORE_SOURCE_DIR}/Profiler/ProfilerHTML.h
# # )

# # set(CORE_MIDDLEWARE_PANINI_SHADER_FILES
# #     ../The-Forge/Middleware_3/PaniniProjection/Shaders/FSL/panini_projection.frag.fsl
# #     ../The-Forge/Middleware_3/PaniniProjection/Shaders/FSL/panini_projection.vert.fsl
# #     ../The-Forge/Middleware_3/PaniniProjection/Shaders/FSL/resources.h
# # )

# # set(CORE_SCRIPTING_FILES
# #     ${CORE_SOURCE_DIR}/Scripting/LuaManager.cpp
# #     ${CORE_SOURCE_DIR}/Scripting/LuaManager.h
# #     ${CORE_SOURCE_DIR}/Scripting/LuaManagerCommon.h
# #     ${CORE_SOURCE_DIR}/Scripting/LuaManagerImpl.cpp
# #     ${CORE_SOURCE_DIR}/Scripting/LuaManagerImpl.h
# #     ${CORE_SOURCE_DIR}/Scripting/LuaSystem.cpp
# #     ${CORE_SOURCE_DIR}/Scripting/LunaV.hpp
# # )

# # set(CORE_UI_FILES
# #     ${CORE_SOURCE_DIR}/UI/UI.cpp
# # )

# # set(CORE_UI_SHADER_FILES
# #     ${CORE_SOURCE_DIR}/UI/Shaders/FSL/imgui.frag.fsl
# #     ${CORE_SOURCE_DIR}/UI/Shaders/FSL/imgui.vert.fsl
# #     ${CORE_SOURCE_DIR}/UI/Shaders/FSL/textured_mesh.frag.fsl
# #     ${CORE_SOURCE_DIR}/UI/Shaders/FSL/textured_mesh.vert.fsl
# # )

# # set(CORE_WINDOWS_FILES
# #     ${CORE_SOURCE_DIR}/Windows/WindowsBase.cpp
# #     ${CORE_SOURCE_DIR}/Windows/WindowsFileSystem.cpp
# #     ${CORE_SOURCE_DIR}/Windows/WindowsLog.c
# #     ${CORE_SOURCE_DIR}/Windows/WindowsStackTraceDump.cpp
# #     ${CORE_SOURCE_DIR}/Windows/WindowsStackTraceDump.h
# #     ${CORE_SOURCE_DIR}/Windows/WindowsThread.c
# #     ${CORE_SOURCE_DIR}/Windows/WindowsTime.c
# # )

# # set(CORE_DARWIN_FILES
# #     ${CORE_SOURCE_DIR}/Darwin/CocoaFileSystem.mm
# #     ${CORE_SOURCE_DIR}/Darwin/DarwinLog.c
# #     ${CORE_SOURCE_DIR}/Darwin/DarwinThread.c
# #     ${CORE_SOURCE_DIR}/FileSystem/UnixFileSystem.cpp
# # )

# # set(CORE_MACCORE_FILES
# #     ${CORE_SOURCE_DIR}/Darwin/macOSBase.mm
# #     ${CORE_SOURCE_DIR}/Darwin/macOSAppDelegate.m
# #     ${CORE_SOURCE_DIR}/Darwin/macOSAppDelegate.h
# #     ${CORE_SOURCE_DIR}/Darwin/macOSWindow.mm
# # )

# # set(CORE_UTILS_FILES
# #     ../The-Forge/Common_3/ThirdParty/OpenSource/EASTL/EASTL.natvis
# # )

# # set(CORE_MIDDLEWARE_ANIMATION_FILES
# #     ../The-Forge/Middleware_3/Animation/AnimatedObject.cpp
# #     ../The-Forge/Middleware_3/Animation/AnimatedObject.h
# #     ../The-Forge/Middleware_3/Animation/Animation.cpp
# #     ../The-Forge/Middleware_3/Animation/Animation.h
# #     ../The-Forge/Middleware_3/Animation/Clip.cpp
# #     ../The-Forge/Middleware_3/Animation/Clip.h
# #     ../The-Forge/Middleware_3/Animation/ClipController.cpp
# #     ../The-Forge/Middleware_3/Animation/ClipController.h
# #     ../The-Forge/Middleware_3/Animation/ClipMask.cpp
# #     ../The-Forge/Middleware_3/Animation/ClipMask.h
# #     ../The-Forge/Middleware_3/Animation/Rig.cpp
# #     ../The-Forge/Middleware_3/Animation/Rig.h
# #     ../The-Forge/Middleware_3/Animation/SkeletonBatcher.cpp
# #     ../The-Forge/Middleware_3/Animation/SkeletonBatcher.h
# # )

# # set(CORE_MIDDLEWARE_PARALLEL_PRIMS_FILES
# #     ../The-Forge/Middleware_3/ParallelPrimitives/ParallelPrimitives.cpp
# #     ../The-Forge/Middleware_3/ParallelPrimitives/ParallelPrimitives.h
# # )

# # set(CORE_WINDOWSYSTEM_FILES
# #     ${CORE_SOURCE_DIR}/WindowSystem/WindowSystem.cpp
# # )

# set(CORE_CORE_SPECIFIC_FILES "")

# source_group(CORE\\Core FILES ${CORE_CORE_FILES})
# source_group(CORE\\FileSystem FILES ${CORE_FILESYSTEM_FILES})
# #source_group(CORE\\Input FILES ${CORE_INPUT_FILES})
# source_group(CORE\\Interfaces FILES ${CORE_INTERFACES_FILES})
# source_group(CORE\\Logging FILES ${CORE_LOGGING_FILES})
# source_group(CORE\\Math FILES ${CORE_MATH_FILES})
# source_group(CORE\\MemoryTracking FILES ${CORE_MEMORYTRACKING_FILES})