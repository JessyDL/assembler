set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CXX_CLANG_TIDY "")

set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF)
set(ASSIMP_IGNORE_GIT_HASH ON)
add_subdirectory(assimp)

find_package(Python3)
include(FetchContent)

if(AS_ENABLE_WGSL)
    set(DAWN_FETCH_DEPENDENCIES ON)

    set(TINT_BUILD_SPV_READER ON)
    set(TINT_BUILD_SPV_WRITER ON)
    set(TINT_BUILD_GLSL_WRITER ON)
    set(TINT_BUILD_WGSL_WRITER ON)

    # We don't need Dawn itself, just Tint.
    set(DAWN_ENABLE_D3D11 OFF)
    set(DAWN_ENABLE_D3D12 OFF)
    set(DAWN_ENABLE_METAL OFF)
    set(DAWN_ENABLE_NULL OFF)
    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(DAWN_ENABLE_VULKAN OFF)
    set(DAWN_USE_GLFW OFF)
    set(DAWN_BUILD_SAMPLES OFF)
    set(DAWN_BUILD_TESTS OFF)
    set(DAWN_BUILD_PROTOBUF OFF)

    # Disable unneeded parts
    set(TINT_BUILD_SAMPLES OFF)
    set(TINT_BUILD_DOCS OFF)
    set(TINT_BUILD_TESTS OFF)
    set(TINT_BUILD_FUZZERS OFF)
    set(TINT_BUILD_SPIRV_TOOLS_FUZZER OFF)
    set(TINT_BUILD_AST_FUZZER OFF)
    set(TINT_BUILD_REGEX_FUZZER OFF)
    set(TINT_BUILD_BENCHMARKS OFF)
    set(TINT_BUILD_AS_OTHER_OS OFF)
    set(TINT_BUILD_REMOTE_COMPILE OFF)
    set(TINT_BUILD_CMD_TOOLS OFF)
    set(TINT_BUILD_IR_BINARY OFF)
    set(TINT_ENABLE_INSTALL ON)

    set(SKIP_SPIRV_TOOLS_INSTALL OFF)

    message(STATUS "Fetching Dawn. Note this will take a long time for the initial setup due to the size of Dawn.")

    # it takes a long time to fetch dawn, although not perfect we will loudly log everything so a user can see it progressing
    set(STORED_FETCHCONTENT_QUIET ${FETCHCONTENT_QUIET})
    set(FETCHCONTENT_QUIET FALSE)
    
    # some of the understandings in making this work are from https://github.com/eliemichel/webgpu-native
    # repo is now defunct, but the information was mostly which parts to enable/disable, and the fetch behaviour (since removed/refactored)
    # after chromium/5931 dawn started having issues with the cmake build and with build times, so we only
    # build the tint part of dawn, which is what we need for WGSL support.
    FetchContent_Declare(
        dawn
        DOWNLOAD_COMMAND
		    cd ${FETCHCONTENT_BASE_DIR}/dawn-src &&
		    git init &&
		    git fetch --depth=1 https://dawn.googlesource.com/dawn chromium/7187 &&
		    git reset --hard FETCH_HEAD &&
            ${Python3_EXECUTABLE} tools/fetch_dawn_dependencies.py -s
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(dawn)
    
    set(FETCHCONTENT_QUIET ${STORED_FETCHCONTENT_QUIET})

    message(STATUS "Dawn now available")
endif()

set(BUILD_TESTING OFF)
set(ENABLE_GLSLANG_BINARIES ON) # todo: future versions will have glslang-default-resource-limits be integrated into glslang propper so we can then turn this off
                                # see issue: https://github.com/KhronosGroup/glslang/issues/3064
set(ENABLE_SPVREMAPPER OFF)
set(ENABLE_HLSL OFF)

set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_AMD_EXTENSIONS ON)
set(ENABLE_NV_EXTENSIONS ON)
set(ENABLE_OPT ON)
set(ENABLE_RTTI ON)

FetchContent_Declare(
        glslang
        DOWNLOAD_COMMAND
		    cd ${FETCHCONTENT_BASE_DIR}/glslang-src &&
		    git init &&
		    git fetch --depth=1 https://github.com/KhronosGroup/glslang.git vulkan-sdk-1.4.321.0 &&
		    git reset --hard FETCH_HEAD &&
            ${Python3_EXECUTABLE} ${FETCHCONTENT_BASE_DIR}/glslang-src/update_glslang_sources.py
        EXCLUDE_FROM_ALL
    )
FetchContent_MakeAvailable(glslang)

set(SPIRV_CROSS_SKIP_INSTALL ON)
add_subdirectory(SPIRV-Cross)

set_target_properties(
    assimp
    UpdateAssimpLibsDebugSymbolsAndDLLs
    zlibstatic
    glslang
    GenericCodeGen
    MachineIndependent
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