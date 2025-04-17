set(ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# include(Shaders)
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)
include(ThirdParty)
include(Runtime)
include(Tests)
