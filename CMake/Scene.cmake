set(SCENE_INTERFACE_DIR ${ENGINE_RUNTIME_SOURCE_DIR}/Scene/Public)
set(SCENE_SOURCE_DIR ${ENGINE_RUNTIME_SOURCE_DIR}/Scene/Private)

file(GLOB_RECURSE SCENE_INTERFACE_FILES ${SCENE_INTERFACE_DIR}/Scene/*.h ${SCENE_INTERFACE_DIR}/Scene/*.hpp)
file(GLOB_RECURSE SCENE_INCLUDE_FILES ${SCENE_SOURCE_DIR}/*.h ${SCENE_SOURCE_DIR}/*.hpp)
file(GLOB_RECURSE SCENE_SOURCE_FILES ${SCENE_SOURCE_DIR}/*.cpp ${SCENE_SOURCE_DIR}/*.c)

# Imported reference code requires a separate shared source tree and is not compiled.
list(FILTER SCENE_SOURCE_FILES EXCLUDE REGEX "/Scene/Private/scene/.*\\.(c|cpp)$")
