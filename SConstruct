import os
import glob
from SCons.Script import *

# Define directories
INCLUDE_DIR = 'include'
LIB_DIR = 'lib'
SRC_DIR = 'src'
TEST_DIR = 'tests'
PLAYGROUND_DIR = 'playground'
BUILD_DIR = 'build'
SHADER_DIR = 'shaders'

# Define source files
LIB_SOURCES = Glob(os.path.join(LIB_DIR, '**', '*.cpp'), strings=True)
MAIN_SOURCE = os.path.join(SRC_DIR, 'main.cpp')
TEST_SOURCES = Glob(os.path.join(TEST_DIR, '*_test.cpp'), strings=True)
PLAYGROUND_SOURCES = Glob(os.path.join(PLAYGROUND_DIR, '*.cpp'), strings=True)
SHADER_SOURCES = Glob(os.path.join(SHADER_DIR, '**', '*.glsl'), strings=True)

# Define compiler and flags
env = Environment(CXX='g++', CXXFLAGS='-std=c++20')

if 'debug' in COMMAND_LINE_TARGETS:
    env.Append(CXXFLAGS=['-g', '-O0'])
    COMMAND_LINE_TARGETS.remove('debug')

# Define include directories
env.Append(CPPPATH=[INCLUDE_DIR])

# Link libraries
env.Append(LIBS=[
    'SDL2',
    'GLEW',
    'GL',
    'GLU',
    'Catch2Main',
    'Catch2',
    'glut',
    'pthread',
    'X11',
    'glfw'
])

# Function to create object files in the build directory
def create_objects(env, sources, build_dir):
    objects = []
    for src in sources:
        obj = env.Object(target=os.path.join(build_dir, 'objects', os.path.splitext(src)[0] + '.o'), source=src)
        objects.append(obj)
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
main_objects = create_objects(env, [MAIN_SOURCE] + LIB_SOURCES, BUILD_DIR)
main_executable = env.Program(target=os.path.join(BUILD_DIR, 'bin', 'AudioSynthesizer'), source=main_objects)

# Build tests and then execute them individually
for target in COMMAND_LINE_TARGETS:
    if 'tests' in target:
        for src in TEST_SOURCES:
            test_name = os.path.splitext(os.path.basename(src))[0]
            test_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR)
            test_executable = env.Program(target=os.path.join(BUILD_DIR, 'tests', test_name), source=test_objects)
            env.Command(
                target=test_executable[0].abspath + '.out',
                source=test_executable,
                action='xvfb-run -a ' + test_executable[0].abspath + ' -d yes > ' + test_executable[0].abspath + '.out 2>&1 && cat ' + test_executable[0].abspath + '.out || (cat ' + test_executable[0].abspath + '.out && false)'
            )

for target in COMMAND_LINE_TARGETS:
    if 'playground' in target:
        for src in PLAYGROUND_SOURCES:
            playground_name = os.path.splitext(os.path.basename(src))[0]
            playground_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR)
            env.Program(target=os.path.join(BUILD_DIR, 'playground', playground_name), source=playground_objects)

# Build docs using doxygen
for target in COMMAND_LINE_TARGETS:
    if 'docs' in target:
        env.Command(
            target=os.path.join(BUILD_DIR, 'docs', 'html', 'index.html'),
            source=Glob('docs/Doxyfile'),
            action='doxygen $SOURCE'
        )
        env.Clean(os.path.join(BUILD_DIR, 'docs', 'html', 'index.html'), Glob(os.path.join(BUILD_DIR, 'docs', 'html', '*'))+Glob(os.path.join(BUILD_DIR, 'docs', 'latex', '*')))

#Specify a single sconsign file for the entire build process
SConsignFile(os.path.join(BUILD_DIR, 'sconsign.dblite'))