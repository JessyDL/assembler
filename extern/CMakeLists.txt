set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CXX_CLANG_TIDY "")

set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF)
set(ASSIMP_IGNORE_GIT_HASH ON)
add_subdirectory(assimp)

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

add_subdirectory(SPIRV-Cross)

set_target_properties(
    assimp
    UpdateAssimpLibsDebugSymbolsAndDLLs
    zlibstatic
    glslang
    glslangValidator
    GenericCodeGen
    MachineIndependent
    OGLCompiler
    OSDependent
    SPIRV
    glslang-default-resource-limits
    spirv-cross
    spirv-cross-c
    spirv-cross-cpp
    spirv-cross-reflect
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-msl
    spirv-cross-util
    spirv-cross-core
    
    PROPERTIES FOLDER "extern")
