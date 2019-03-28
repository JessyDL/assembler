from argparse import ArgumentParser
import os 
import sys 
import platform
import shutil
import subprocess

dir_path = os.path.dirname(os.path.realpath(__file__))

parser = ArgumentParser(description='Generate build files for the current project.')
parser.add_argument("-g", "--generator", const="auto", default="auto", nargs='?',
                    help="Set the generator for the project (MSVC, Make, or Ninja)", dest="generator")
parser.add_argument("-c", "--compiler", const="auto", default="auto", nargs='?',
                    help="Set the compiler for the project (MSVC or CLang)", dest="compiler")
parser.add_argument("--arch", const="x64", default="x64", nargs='?', 
                    help="Architecture type to generate for", dest="architecture")
parser.add_argument("--clean", const="all", default="", nargs='?', 
                    help="Allows you to clean the project files, build files, or all", dest="clean")
parser.add_argument("--root", const=dir_path, default=dir_path, nargs='?', 
                    help="Override for the root directory to work from", dest="root_dir")
parser.add_argument("--build_dir", const="builds", default="builds", nargs='?', 
                    help="Override for the build directory, this is relative to the root", dest="build_dir")
parser.add_argument("--project_dir", const="project_files", default="project_files", nargs='?', 
                    help="Override for the project files directory, this is relative to the root", dest="project_dir")
parser.add_argument("--vk_version", const="1.1.82.1", default="1.1.82.1", nargs='?', 
                    help="vulkan version to use", dest="vk_version")
parser.add_argument("--vk_root", const="auto", default="auto", nargs='?',
                    help="root directory for vulkan", dest="vk_root")
parser.add_argument("--vk_static", action='store_true',dest="vk_static", help="when this flag is set, vulkan will statically bind")
parser.add_argument("-u","--update", action='store_true', dest="cmake_update")
parser.add_argument("-v","--verbose", action='store_true', dest="verbose")
parser.add_argument("--cmake_params", nargs='*',dest="cmake_params")
args = parser.parse_args()

if args.verbose:
    print(args)

# validating
args.generator = args.generator.lower()
if args.generator == 'auto':
    args.generator = 'msvc'

cmake_generator = "ninja"
if args.generator == "msvc":
    if args.architecture == 'x64':
        cmake_generator = "Visual Studio 15 2017 Win64"
    elif args.architecture == 'arm':
        cmake_generator = "Visual Studio 15 2017 ARM"
elif args.generator == 'make':
    cmake_generator = "Unix Makefiles"
		
args.compiler = args.compiler.lower

if args.vk_root == 'auto':
    if platform.system() == 'Windows':
	      args.vk_root = 'C:/VulkanSDK'
    elif platform.system() == 'Linux':
        args.ck_root = '/VulkanSDK/'
    else:
        raise Exception("could not determine the OS")
		
if not os.path.isdir(args.vk_root):
    print("ERROR: vulkan root directory '{0}' could not be found, please set '--vk_root'".format(args.vk_root))
    raise Exception("InvalidVulkanDirectory")

vk_dir = args.vk_root + os.path.sep + args.vk_version
if not os.path.isdir(vk_dir):
    print("ERROR: vulkan version '{0}' could not be found at '{1}', please set '--vk_version'".format(args.vk_version, vk_dir))
    raise Exception("InvalidVulkanVersion")

project_dir = os.path.join(args.root_dir, args.project_dir, args.generator, args.architecture)
build_dir = os.path.join(args.root_dir, args.build_dir, args.generator, args.architecture)

if not os.path.exists(project_dir):
    os.makedirs(project_dir)

if not os.path.exists(build_dir):
    os.makedirs(build_dir)

if args.clean == 'all' or args.clean == 'build':
    print("cleaning: {0}".format(build_dir))
    for the_file in os.listdir(build_dir):
        file_path = os.path.join(build_dir, the_file)
        try:
            if os.path.isfile(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path): shutil.rmtree(file_path)
        except Exception as e:
            print(e)
						
if args.clean == 'all' or args.clean == 'project':
    print("cleaning: {0}".format(project_dir))
    for the_file in os.listdir(project_dir):
        file_path = os.path.join(project_dir, the_file)
        try:
            if os.path.isfile(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path): shutil.rmtree(file_path)
        except Exception as e:
            print(e)
						
if not os.path.exists(os.path.join(project_dir, "CMakeFiles")):
    vk_static = "OFF"
    print("generating project files")
    if args.vk_static:
        vk_static = "ON"
    cmakeCmd = ["cmake.exe", "-G", cmake_generator, "-DPE_BUILD_DIR="+build_dir, "-DVK_VERSION="+args.vk_version, "-DVK_ROOT="+args.vk_root, "-DVK_STATIC="+vk_static]
    if args.cmake_params:
        cmakeCmd = cmakeCmd + args.cmake_params
    cmakeCmd = cmakeCmd + [ "-H"+ args.root_dir, "-B"+project_dir]
    retCode = subprocess.check_call(cmakeCmd, shell=sys.platform.startswith('win'))
elif args.cmake_update:
    cmakeCmd = ["cmake.exe", r"."] + args.cmake_params
    retCode = subprocess.check_call(cmakeCmd, shell=sys.platform.startswith('win'))