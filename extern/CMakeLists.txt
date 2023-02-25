set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CXX_CLANG_TIDY "")

add_subdirectory(assimp)
set_property(TARGET assimp PROPERTY FOLDER "extern")

set(BUILD_TESTING OFF)
set(ENABLE_GLSLANG_BINARIES ON) # todo: future versions will have glslang-default-resource-limits be integrated into glslang propper so we can then turn this off
                                # see issue: https://github.com/KhronosGroup/glslang/issues/3064
set(ENABLE_SPVREMAPPER OFF)
set(ENABLE_HLSL OFF)

set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_AMD_EXTENSIONS ON)
set(ENABLE_NV_EXTENSIONS ON)
set(ENABLE_OPT ON)

add_subdirectory(glslang)
set_property(TARGET glslang PROPERTY FOLDER "extern")

set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(SPIRV-Cross)
