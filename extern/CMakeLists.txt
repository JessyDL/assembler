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
set(ENABLE_RTTI ON)

add_subdirectory(glslang)

find_package(Python3)

# if this is the first time we are building glslang, we need to run `update_glslang_sources.py`
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/update_glslang_sources.py.timestamp
  COMMAND ${CMAKE_COMMAND} -E echo "glslang dependencies changed" > ${CMAKE_CURRENT_BINARY_DIR}/update_glslang_sources.py.timestamp
  COMMAND ${CMAKE_COMMAND} -E echo "Updating glslang sources..."
  COMMAND ${Python3_EXECUTABLE} ${glslang_SOURCE_DIR}/update_glslang_sources.py
  WORKING_DIRECTORY ${glslang_SOURCE_DIR}
  DEPENDS ${glslang_SOURCE_DIR}/update_glslang_sources.py
)

add_custom_target(UpdateGlslangSources
    DEPENDS update_glslang_sources.py.timestamp
)

add_dependencies(glslang
    UpdateGlslangSources
)

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

if(AS_ENABLE_WGSL)
    set(DAWN_FETCH_DEPENDENCIES ON) # use manual method so we don't want to pull in depot_tools

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

    
    # some of the understandings in making this work are from https://github.com/eliemichel/webgpu-native
    # after chromium/5931 dawn started having issues with the cmake build and with build times.
    include(FetchContent)

    FetchContent_Declare(
	dawn
	DOWNLOAD_COMMAND
		cd ${FETCHCONTENT_BASE_DIR}/dawn-src &&
		git init &&
		git fetch --depth=1 https://dawn.googlesource.com/dawn chromium/7187 &&
		git reset --hard FETCH_HEAD
    )
    
    FetchContent_MakeAvailable(dawn)

    # add_subdirectory(${dawn_SOURCE_DIR} ${dawn_BINARY_DIR} EXCLUDE_FROM_ALL)
    # target_include_directories(dawn_shared_utils PUBLIC "${CMAKE_BINARY_DIR}/_deps/dawn-src/src")
endif()
