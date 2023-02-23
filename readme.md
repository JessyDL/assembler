![Language: C++20](https://img.shields.io/badge/language-c%2B%2B20-blue.svg?longCache=true&style=for-the-badge) ![Using: Vulkan](https://img.shields.io/badge/using-vulkan%201.1-red.svg?longCache=true&style=for-the-badge) ![Unit Test Status](https://img.shields.io/badge/status-passed%20all%20tests-green.svg?longCache=true&style=for-the-badge)  ![Git last commit](https://img.shields.io/github/last-commit/JessyDL/assembler.svg?style=for-the-badge)
# Assembler
The `assembler` project is a Command Line Interface tool to manipulate and generate various data structures that the engine can consume. Using `assembler` you can compile `shaders` that generates their companion [core::meta::shaders](https://jessydl.github.io/paradigm/classcore_1_1meta_1_1shader.html), import models using  [assimp v4.1.0](https://github.com/assimp/assimp/releases/tag/v4.1.0/), setup a [meta::library](https://jessydl.github.io/paradigm/classmeta_1_1library.html), generate [meta::file](https://jessydl.github.io/paradigm/classmeta_1_1file.html)s, and more. 

## Prerequisites
[CMake ]( http://cmake.org/) 3.11 or higher is required on all platforms.

Transitive requirements from dependencies (particularly `paradigm`'s dependencies)

## Building
Note that the `paradigm` submodule heavily influences the building of the project.

**using `build.py`**
The build script is a helper script to set everything up quick and easy. It will generate a solution in the `/project_file/{generator}/{architecture}/` folder by default, and when building it will output to `/builds/{generator}/{architecture}/`.
You can tweak various settings and values, you'll find them at the top of the `build.py` file.
#### cmake
The less easy way, but perhaps better and easier to integrate into existing projects. The values that are required to be set can be seen in the cmake invocation in build.py (and in the paradigm submodule), but will be repeated here:
-`DBUILD_DIRECTORY="path/to/where/to/build/to"` (not to be confused with where the project files will be)
-`DPE_VULKAN_VERSION`: string based value that is a 1-1 match with an available tag in the [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) repository. These will be downloaded and used.
-`DPE_VULKAN` toggle value to enable/disable vulkan backend. (default ON)
-`DPE_GLES` toggle value to enable/disable gles backend (default ON)

## Running
After building the project, it should be automatically set up to run. When executing you should be greeted with a screen similar to this (after executing the `--help` command).
<img src="https://raw.githubusercontent.com/JessyDL/assembler/master/output.png " width=600>
## Commands
`assembler` can be used in 2 modes, either directly by starting up the binary, or by invoking it from another command line tool and sending it arguments. When unsure what to do, write `--help` or `-h`, and information will be printed on the current actions that can be done. To quit, write `--quit` or `-q`.

When invoking from a script, or through another command line tool or terminal, and you want to send multiple parameters, use the ` | ` symbol to pipe together commands.

Be sure not to forget to send the `--quit` command when executing from a script or terminal. The default mode is `interactive` and so it will check for `std::cin` while it has not received a clear quit command.
## Dependencies
There are also various external dependencies used, the following is the list of all direct external dependencies `assembler` has. It is assumed the dependency version is the latest available unless explicitly stated.
Many of these dependencies also pull in more dependencies. Verify on the project pages directly what these are.
- [assimp 4.1.0](https://github.com/assimp/assimp)
- [glslang](https://github.com/KhronosGroup/glslang)
- [paradigm](https://github.com/JessyDL/paradigm)

# License
The `assembler` project, and all code below this file's tree is licensed under GNU AGPLv3. As long as attribution is added of what the original source is, and a link to this repository (https://github.com/JessyDL/assembler).
