import os
import glob
import re
from SCons.Script import *

# Define directories
INCLUDE_DIR = 'include'
LIB_DIR = 'lib'
SRC_DIR = 'src'
TEST_DIR = 'tests'  # Old test directory
PLAYGROUND_DIR = 'playground'
BUILD_DIR = 'build'
SHADER_DIR = 'shaders'

# Define source files
LIB_SOURCES = Glob(os.path.join(LIB_DIR, '**', '*.cpp'), strings=True)
MAIN_SOURCE = os.path.join(SRC_DIR, 'main.cpp')
TEST_SOURCES = Glob(os.path.join(TEST_DIR, '*_test.cpp'), strings=True)  # Old test sources
PLAYGROUND_SOURCES = Glob(os.path.join(PLAYGROUND_DIR, '*.cpp'), strings=True)
SHADER_SOURCES = Glob(os.path.join(SHADER_DIR, '**', '*.glsl'), strings=True)

# Define compiler and flags
env = Environment(CXX='g++', CXXFLAGS='-std=c++20')

AddOption('--gdb',
          dest='debug',
          action='store_true',
          default=False,
          help='Build with debug flags (-g -O0)')
          
# Add option for directly specifying tests
AddOption('--test',
          dest='test',
          type='string',
          nargs='?',
          action='store',
          metavar='TEST_NAME',
          help='Specify a test to build and run (e.g. "--test audio_buffer_test"). If no test name provided, runs all tests.')

if GetOption('debug'):
    env.Append(CXXFLAGS=['-g', '-O0'])
    env.Append(LINKFLAGS=['-g'])

# Define include directories
env.Append(CPPPATH=[INCLUDE_DIR, '.'])  # Include the root directory for test framework access

# Link libraries for main executable
env.Append(LIBS=[
    'SDL2_image',
    'SDL2_mixer',
    'SDL2_gfx',
    'SDL2_ttf',
    'SDL2',
    'GLEW',
    'GL',
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

# Copy shaders to build directory
all_shaders = []
for src in SHADER_SOURCES:
    all_shaders += env.Command(target=os.path.join(BUILD_DIR, 'shaders', os.path.basename(src)), source=src, action='cp $SOURCE $TARGET')

# renderer and render stages depend on shaders
env.Depends(LIB_SOURCES, all_shaders)

# Build main executable
main_objects = create_objects(env, [MAIN_SOURCE] + LIB_SOURCES, BUILD_DIR, 'main')
main_executable = env.Program(target=os.path.join(BUILD_DIR, 'bin', 'AudioSynthesizer'), source=main_objects)

# Function to build and run all tests
def build_all_tests(env):
    # Get all test files from tests directory - looking directly in the tests dir (not in subdirectories)
    TEST_SOURCES = Glob(os.path.join('tests', '*_test.cpp'), strings=True)
    
    # Create a main test file that includes all test files
    main_test_src = os.path.join('tests', 'test_main.cpp')
    
    # Include all test files EXCEPT the main one in our test objects
    test_files_without_main = [src for src in TEST_SOURCES if os.path.basename(src) != 'test_main.cpp']
    
    print(f"Building all tests with {len(test_files_without_main)} test files:")
    for test_file in test_files_without_main:
        print(f"  - {os.path.basename(test_file)}")
    
    # Build the all-in-one test executable
    test_objects = create_objects(env, [main_test_src] + test_files_without_main + LIB_SOURCES, BUILD_DIR, 'tests')
    all_tests_executable = env.Program(target=os.path.join(BUILD_DIR, 'tests', 'all_tests'), source=test_objects)
    
    # Create a command to run all tests
    # Check if we're running a specific test
    specified_test = GetOption('test')
    test_filter = ""
    if specified_test and specified_test != '':
        # Add a test name filter to the Catch2 command line
        # In Catch2, we need to use the format [tag] to filter by test case tag
        test_filter = f" --test-case \"{specified_test}\""
    
    test_output = env.Command(
        target=all_tests_executable[0].abspath + '.out',
        source=all_tests_executable,
        action=all_tests_executable[0].abspath + test_filter + ' -d yes > ' + all_tests_executable[0].abspath + '.out 2>&1 && cat ' + all_tests_executable[0].abspath + '.out || (cat ' + all_tests_executable[0].abspath + '.out && false)'
    )
    
    # Create an alias to run all tests
    env.Alias('tests', test_output)
    
    return all_tests_executable, test_output

# Check if we're building and running tests
build_tests = 'tests' in COMMAND_LINE_TARGETS or GetOption('test') is not None

# Build all tests if needed
if build_tests:
    all_tests_executable, test_output = build_all_tests(test_env)
    # Make 'tests' the default target if --test was specified
    Default(test_output)

# Support for building specific tests directly
# Get all test files automatically from the tests directory
all_test_files = Glob(os.path.join('tests', '*_test.cpp'), strings=True)
specific_tests = []
for test_file in all_test_files:
    test_name = os.path.splitext(os.path.basename(test_file))[0]
    specific_tests.append(test_name)

# Add manually any additional tests that might not follow the naming convention
additional_tests = [
    'audio_renderer_basic_test'  # Only if not already covered by the glob pattern
]
for test in additional_tests:
    if test not in specific_tests:
        specific_tests.append(test)

# Check for --test option
specified_test = GetOption('test')
if specified_test is not None:
    if specified_test == '':  # No test name provided, run all tests
        print("Running all tests...")
        # Make sure 'tests' is in COMMAND_LINE_TARGETS to build and run all tests
        if 'tests' not in COMMAND_LINE_TARGETS:
            COMMAND_LINE_TARGETS.append('tests')
    else:  # Specific test name provided
        print(f"Running specific test: {specified_test}")
        # Instead of building individual tests, we'll filter tests in the all_tests executable
        if 'tests' not in COMMAND_LINE_TARGETS:
            COMMAND_LINE_TARGETS.append('tests')

# Support for building individual tests directly
for specific_test in specific_tests:
    test_src = os.path.join('tests', specific_test + '.cpp')
    if os.path.exists(test_src) and specific_test in COMMAND_LINE_TARGETS:
        # Build individual test with test_main.cpp
        test_objects = create_objects(test_env, [os.path.join('tests', 'test_main.cpp'), test_src] + LIB_SOURCES, BUILD_DIR, 'tests')
        test_executable = test_env.Program(target=os.path.join(BUILD_DIR, 'tests', specific_test), source=test_objects)
        
        # Run individual test
        test_output = test_env.Command(
            target=test_executable[0].abspath + '.out',
            source=test_executable,
            action=test_executable[0].abspath + ' -d yes > ' + test_executable[0].abspath + '.out 2>&1 && cat ' + test_executable[0].abspath + '.out || (cat ' + test_executable[0].abspath + '.out && false)'
        )
        
        # Create an alias to build and run this test
        test_env.Alias(specific_test, test_output)

# Build all playground examples
if 'playground' in COMMAND_LINE_TARGETS:
    for src in PLAYGROUND_SOURCES:
        playground_name = os.path.splitext(os.path.basename(src))[0]
        playground_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR, 'playground')
        env.Program(target=os.path.join(BUILD_DIR, 'playground', playground_name), source=playground_objects)

# Support for building specific playground examples
for target in COMMAND_LINE_TARGETS:
    if target.startswith('playground/'):
        playground_name = target.split('/', 1)[1]
        src = os.path.join(PLAYGROUND_DIR, playground_name + '.cpp')
        if os.path.exists(src):
            playground_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR, 'playground')
            env.Program(target=os.path.join(BUILD_DIR, 'playground', playground_name), source=playground_objects)

# Build docs using doxygen
if 'docs' in COMMAND_LINE_TARGETS:
    env.Command(
        target=os.path.join(BUILD_DIR, 'docs', 'html', 'index.html'),
        source=Glob('docs/Doxyfile'),
        action='doxygen $SOURCE'
    )
    env.Clean(os.path.join(BUILD_DIR, 'docs', 'html', 'index.html'), Glob(os.path.join(BUILD_DIR, 'docs', 'html', '*'))+Glob(os.path.join(BUILD_DIR, 'docs', 'latex', '*')))

#Specify a single sconsign file for the entire build process
SConsignFile(os.path.join(BUILD_DIR, 'sconsign.dblite'))