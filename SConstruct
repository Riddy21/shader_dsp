import os
import glob
import re
from SCons.Script import *

# Define directories
INCLUDE_DIR = 'include'
LIB_DIR = 'lib'
SRC_DIR = 'src'
TEST_DIR = 'tests'
TEST_FRAMEWORK_DIR = os.path.join(TEST_DIR, 'framework')
PLAYGROUND_DIR = 'playground'
BUILD_DIR = 'build'
SHADER_DIR = 'shaders'

# FIXME: have playground tests and scripts share the same build so files
# Define source files
LIB_SOURCES = Glob(os.path.join(LIB_DIR, '**', '*.cpp'), strings=True)
MAIN_SOURCE = os.path.join(SRC_DIR, 'main.cpp')
TEST_SOURCES = Glob(os.path.join(TEST_DIR, '*_test.cpp'), strings=True) + Glob(os.path.join(TEST_FRAMEWORK_DIR, '*_test.cpp'), strings=True)
PLAYGROUND_SOURCES = Glob(os.path.join(PLAYGROUND_DIR, '*.cpp'), strings=True)
SHADER_SOURCES = Glob(os.path.join(SHADER_DIR, '**', '*.glsl'), strings=True)

# Define command-line options
AddOption('--gdb',
          dest='debug',
          action='store_true',
          default=False,
          help='Build with debug flags (-g -O0)')

AddOption('--all-tests',
          dest='all_tests',
          action='store_true',
          default=False,
          help='Build and run all tests')

AddOption('--test',
          dest='test',
          type='string',
          action='store',
          metavar='TEST_NAME',
          help='Build and run a specific test (e.g. "--test=audio_buffer_test")')

AddOption('--test-case',
          dest='test_case',
          type='string',
          action='store',
          metavar='TEST_CASE_NAME',
          help='Run a specific test case within a test (e.g. "--test-case=\"AudioTexture2DParameter with OpenGL context\"")')

AddOption('--playground',
          dest='playground',
          type='string',
          action='store',
          metavar='EXAMPLE_NAME',
          help='Build playground examples. Optionally specify an example name (e.g. "--playground=audio_generator_test").')

AddOption('--docs',
          dest='docs',
          action='store_true',
          default=False,
          help='Build documentation using Doxygen.')

AddOption('--section',
          dest='section',
          type='string',
          action='store',
          metavar='SECTION_NAME',
          help='Run a specific section within a test case (e.g. --section="RGBA32F, 256x1, OUTPUT")')

# Define compiler environment
env = Environment(CXX='g++', CXXFLAGS='-std=c++20')

# Configure debug build if requested
if GetOption('debug'):
    env.Append(CXXFLAGS=['-g', '-O0'])
    env.Append(LINKFLAGS=['-g'])

# Define include directories
env.Append(CPPPATH=[INCLUDE_DIR, '.'])  # Include the root directory for test framework access

# Add SDL2 include paths - try multiple common locations
sdl2_include_paths = [
    '/usr/include/SDL2',
    '/usr/local/include/SDL2',
    '/opt/homebrew/include/SDL2',
    '/usr/include',
    '/usr/local/include',
    '/usr/include/x86_64-linux-gnu/SDL2',
    '/usr/include/i386-linux-gnu/SDL2'
]

for path in sdl2_include_paths:
    if os.path.exists(path):
        env.Append(CPPPATH=[path])

# Try to use pkg-config for SDL2 if available
try:
    import subprocess
    sdl2_cflags = subprocess.check_output(['pkg-config', '--cflags', 'sdl2'], universal_newlines=True).strip()
    if sdl2_cflags:
        env.Append(CPPFLAGS=sdl2_cflags.split())
except (subprocess.CalledProcessError, FileNotFoundError):
    pass  # pkg-config not available or SDL2 not found

# Add OpenGL ES 3.0 include paths
env.Append(CPPPATH=['/usr/include/GLES3', '/usr/include/GLES2', '/usr/include/EGL'])

# Link libraries for main executable - use OpenGL ES instead of regular OpenGL
env.Append(LIBS=[
    'SDL2_image',
    'SDL2_mixer',
    'SDL2_gfx',
    'SDL2_ttf',
    'SDL2',
    'GLESv2',  # OpenGL ES 2.0/3.0 library
    'GLESv1_CM',  # OpenGL ES 1.x library
    'EGL',  # EGL library
    'pthread',
    'X11',
])

# Create separate environment for tests with Catch2
test_env = env.Clone()
test_env.Append(LIBS=['Catch2Main', 'Catch2'])
test_env.Append(CXXFLAGS=['-fno-access-control'])  # Allow access to private members

# Function to create object files in the build directory
def create_objects(env, sources, build_dir, subdir):
    objects = []
    for src in sources:
        obj_path = os.path.join(build_dir, 'objects', subdir, os.path.splitext(src)[0] + '.o')
        objects.append(env.Object(target=obj_path, source=src))
    return objects

# Create build directory
VariantDir(BUILD_DIR, '.', duplicate=0)

# Copy shaders to a single directory in the build directory
SHADER_BUILD_DIR = os.path.join(BUILD_DIR, 'shaders')
all_shaders = []
for src in SHADER_SOURCES:
    dest_path = os.path.join(SHADER_BUILD_DIR, os.path.basename(src))
    all_shaders += env.Command(target=dest_path, source=src, action=Mkdir(SHADER_BUILD_DIR) + 'cp $SOURCE $TARGET')

# renderer and render stages depend on shaders
env.Depends(LIB_SOURCES, all_shaders)

# Function to build and run tests
def build_tests(env, specific_test=None, test_case=None, section=None):
    # Get all test files from tests directory and framework subdirectory
    test_files = Glob(os.path.join(TEST_DIR, '*_test.cpp'), strings=True)
    framework_test_files = Glob(os.path.join(TEST_FRAMEWORK_DIR, '*_test.cpp'), strings=True) if os.path.exists(TEST_FRAMEWORK_DIR) else []
    
    # Add the test files from the framework directory
    all_test_files = test_files + framework_test_files
    
    # Main test entry point
    main_test_src = os.path.join(TEST_FRAMEWORK_DIR, 'test_main.cpp')
    if not os.path.exists(main_test_src):
        print(f"Warning: test_main.cpp not found at {main_test_src}")
        return None
    
    # Filter out test_main.cpp if it's in the list
    test_files_without_main = [src for src in all_test_files if os.path.basename(src) != 'test_main.cpp']
    
    # If a specific test is requested
    if specific_test:
        # Check if the specific test exists in either directory
        specific_test_file = None
        for test_file in test_files_without_main:
            if os.path.splitext(os.path.basename(test_file))[0] == specific_test:
                specific_test_file = test_file
                break
        
        if specific_test_file:
            print(f"Building and running specific test: {specific_test}")
            test_objects = create_objects(env, [main_test_src, specific_test_file] + LIB_SOURCES, BUILD_DIR, 'tests')
            test_executable = env.Program(target=os.path.join(BUILD_DIR, 'tests', specific_test), source=test_objects)
            
            # Create a temp directory for XDG_RUNTIME_DIR
            xdg_runtime_dir = '/tmp/xdg-runtime-dir'
            
            # Set up the command with XDG_RUNTIME_DIR
            test_command = f'mkdir -p {xdg_runtime_dir} && XDG_RUNTIME_DIR={xdg_runtime_dir} '
            test_command += 'xvfb-run -a ' + test_executable[0].abspath + ' -d yes'
            
            # Add test case filter if specified
            if test_case:
                test_command += f" '{test_case}'"
            
            if section:
                test_command += f" -c '{section}'"
            
            test_command += ' > ' + test_executable[0].abspath + '.out 2>&1 && cat ' + test_executable[0].abspath + '.out || (cat ' + test_executable[0].abspath + '.out && false)'
            
            test_output = env.Command(
                target=test_executable[0].abspath + '.out',
                source=test_executable,
                action=test_command
            )
            
            env.Alias('test-' + specific_test, test_output)
            return test_output
        else:
            print(f"Test '{specific_test}' not found. Building and running all tests instead.")
    
    # Build all tests
    print(f"Building all tests with {len(test_files_without_main)} test files:")
    for test_file in test_files_without_main:
        print(f"  - {os.path.basename(test_file)}")
    
    # Build the all-in-one test executable
    test_objects = create_objects(test_env, [main_test_src] + test_files_without_main + LIB_SOURCES, BUILD_DIR, 'tests')
    all_tests_executable = test_env.Program(target=os.path.join(BUILD_DIR, 'tests', 'all_tests'), source=test_objects)
    
    # Create a command to run all tests
    test_filter = ""
    if specific_test:
        # Add a test name filter to the Catch2 command line
        test_filter = f" '{specific_test}'"
    elif test_case:
        # Add a test case filter to the Catch2 command line
        test_filter = f" '{test_case}'"
    
    if section:
        test_filter += f" -c '{section}'"
    
    # Create a temp directory for XDG_RUNTIME_DIR
    xdg_runtime_dir = '/tmp/xdg-runtime-dir'
    
    # Set up the command with XDG_RUNTIME_DIR for xvfb-run
    test_command = f'mkdir -p {xdg_runtime_dir} && XDG_RUNTIME_DIR={xdg_runtime_dir} '
    test_command += 'xvfb-run -a ' + all_tests_executable[0].abspath + test_filter + ' -d yes > ' + all_tests_executable[0].abspath + '.out 2>&1 && cat ' + all_tests_executable[0].abspath + '.out || (cat ' + all_tests_executable[0].abspath + '.out && false)'
    
    test_output = test_env.Command(
        target=all_tests_executable[0].abspath + '.out',
        source=all_tests_executable,
        action=test_command
    )
    
    env.Alias('all-tests', test_output)
    return test_output

# Function to build playground examples
def build_playground(env, specific_example=None):
    if specific_example:
        src = os.path.join(PLAYGROUND_DIR, specific_example + '.cpp')
        if os.path.exists(src):
            print(f"Building playground example: {specific_example}")
            playground_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR, 'playground')
            return env.Program(target=os.path.join(BUILD_DIR, 'playground', specific_example), source=playground_objects)
        else:
            print(f"Playground example '{specific_example}' not found.")
            return None
    else:
        print(f"Building all playground examples:")
        playground_targets = []
        for src in PLAYGROUND_SOURCES:
            playground_name = os.path.splitext(os.path.basename(src))[0]
            print(f"  - {playground_name}")
            playground_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR, 'playground')
            playground_targets.append(env.Program(target=os.path.join(BUILD_DIR, 'playground', playground_name), source=playground_objects))
        return playground_targets

# Function to build documentation
def build_docs(env):
    docs_target = env.Command(
        target=os.path.join(BUILD_DIR, 'docs', 'html', 'index.html'),
        source=Glob('docs/Doxyfile'),
        action='doxygen $SOURCE'
    )
    env.Clean(docs_target, Glob(os.path.join(BUILD_DIR, 'docs', 'html', '*'))+Glob(os.path.join(BUILD_DIR, 'docs', 'latex', '*')))
    return docs_target

# Determine build targets based on command-line options
targets = []

# Handle --all-tests option (build all tests)
if GetOption('all_tests'):
    test_targets = build_tests(test_env)
    if test_targets:
        targets.append(test_targets)

# Handle --test option (build specific test)
test_name = GetOption('test')
test_case = GetOption('test_case')
section = GetOption('section')
if test_name:
    test_targets = build_tests(test_env, test_name, test_case, section)
    if test_targets:
        targets.append(test_targets)
elif test_case or section:
    # If only --test-case or --section is specified, run all tests but filter by test case/section
    test_targets = build_tests(test_env, None, test_case, section)
    if test_targets:
        targets.append(test_targets)

# Handle --playground option
playground_option = GetOption('playground')
if playground_option is not None:
    playground_name = playground_option if playground_option != '' else None
    playground_targets = build_playground(env, playground_name)
    if playground_targets:
        targets.extend(playground_targets if isinstance(playground_targets, list) else [playground_targets])

# Handle --docs option
if GetOption('docs'):
    docs_target = build_docs(env)
    targets.append(docs_target)

# Default to building the main executable if no options specified
if not targets:
    main_objects = create_objects(env, [MAIN_SOURCE] + LIB_SOURCES, BUILD_DIR, 'main')
    main_executable = env.Program(target=os.path.join(BUILD_DIR, 'bin', 'AudioSynthesizer'), source=main_objects)
    targets.append(main_executable)

Default(targets)

# Specify a single sconsign file for the entire build process
SConsignFile(os.path.join(BUILD_DIR, 'sconsign.dblite'))