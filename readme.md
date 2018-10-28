![Language: C++17](https://img.shields.io/badge/language-c%2B%2B17-blue.svg?longCache=true&style=for-the-badge) ![Using: Vulkan](https://img.shields.io/badge/using-vulkan%201.1-red.svg?longCache=true&style=for-the-badge) ![Unit Test Status](https://img.shields.io/badge/status-passed%20all%20tests-green.svg?longCache=true&style=for-the-badge)  ![Git last commit](https://img.shields.io/github/last-commit/JessyDL/assembler.svg?style=for-the-badge)
# Assembler
The `assembler` project is a Command Line Interface tool to manipulate and generate various data structures that the engine can consume. Using `assembler` you can compile `shaders` that generates their companion [core::meta::shaders](https://jessydl.github.io/paradigm/classcore_1_1meta_1_1shader.html), import models using  [assimp v4.1.0](https://github.com/assimp/assimp/releases/tag/v4.1.0/), setup [meta::library](https://jessydl.github.io/paradigm/classmeta_1_1library.html)'s meta::library's and the [meta::file](https://jessydl.github.io/paradigm/classmeta_1_1file.html)'s, and more. 



## Prerequisites
To use `assembler` on Windows, Bash on Ubuntu on Windows should be installed.
[CMake ]( http://cmake.org/) 3.11 or higher is required on all platforms.
[assimp v4.1.0](https://github.com/assimp/assimp/releases/tag/v4.1.0/).
## Building

**using `build.sh`**
Depending on how you installed the Vulkan SDK, the only thing you'll *need* to set is `VULKAN_VERSION`. If you have version 1.1.70.0 installed, then that will need to be invoked as `-VULKAN_VERSION="1.1.70.0"`. If for some reason the build script can't find the Vulkan SDK, you can set the root location (without the version in the path) using the `-VULKAN_ROOT="path/to/sdk"`.

You can set the cmake generator using the shorthand `-G`, which works identical to the cmake variant.

To set the assimp path, you can **end** your `build.sh` invocation with `-cmake_params "-DASSIMP_PATH='path/to/assimp'"`
cmake_params needs to be at the end, because everything after will be ignored.

**using cmake directly**
It is perfectly possible to use cmake directly instead, as long as you set the following values.

`-DBUILD_DIRECTORY="path/to/where/to/build/to"` (not to be confused with where the project files will be)
`-DVULKAN_ROOT="path/to/sdk"` path to install location of the vulkan SDK (excluding the version part).
`-DVULKAN_VERSION="1.1.70.0"` string based value that is a 1-1 match with a installed vulkan SDK instance.
`-DASSIMP_PATH='path/to/assimp'` this is the root folder of your assimp sdk installation.

## Running
When executing the compiled assembler binary, you will need to copy the items present in the project's `libraries` folder otherwise you will end up with an error on boot about missing libraries. After that's done, you should be greeted with a screen similar to this (after executing the `--help` command).
![output.png]( https://raw.githubusercontent.com/JessyDL/assembler/master/output.png =600x)
## Commands
`assembler` can be used in 2 modes, either directly by starting up the binary, or by invoking it from another command line tool and sending it arguments. When unsure what to do, write `--help` or `-h`, and information will be printed on the current actions that can be done. To quit, write `--quit` or `-q`.

When invoking from a script, or through another command line tool or terminal, and you want to send multiple parameters, use the ` | ` symbol to pipe together commands.

Be sure not to forget to send the `--quit` command when executing from a script or terminal. The default mode is `interactive` and so it will check for `std::cin` while it has not received a clear quit command.
## Dependencies
![](https://img.shields.io/badge/core-passed%20all%20tests-green.svg?longCache=true&style=flat-square) ![](https://img.shields.io/badge/common-passed%20all%20tests-green.svg?longCache=true&style=flat-square) ![](https://img.shields.io/badge/format-passed%20all%20tests-green.svg?longCache=true&style=flat-square) ![](https://img.shields.io/badge/utility-passed%20all%20tests-green.svg?longCache=true&style=flat-square) ![](https://img.shields.io/badge/meta-passed%20all%20tests-green.svg?longCache=true&style=flat-square)
### Externals
There are also various external dependencies used, the following is the list of all direct external dependencies `assembler` has.
- Assimp 4.1.0
- Paradigm Engine

# License
The `assembler` project, and all code below this file's tree is licensed under GNU AGPLv3. As long as attribution is added of what the original source is, and a link to this repository.
