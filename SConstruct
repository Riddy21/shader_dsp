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
SHADER_SOURCES = Glob(os.path.join(SHADER_DIR, '*'), strings=True)

# Define compiler and flags
env = Environment(CXX='g++', CXXFLAGS='-std=c++20')

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
    'portaudio',
    'pthread'
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
for src in SHADER_SOURCES:
    shaders = env.Command(target=os.path.join(BUILD_DIR, 'shaders', os.path.basename(src)), source=src, action='cp $SOURCE $TARGET')

# renderer and render stages depend on shaders
render_stage_sources = Glob(os.path.join(SRC_DIR, 'renderer', 'render_stages', '*.cpp'), strings=True)
env.Depends(render_stage_sources, shaders)

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
            env.Command(target=test_executable[0].abspath + '_output.txt', source=test_executable, action='xvfb-run -a ' + test_executable[0].abspath + ' -d yes > ' + test_executable[0].abspath + '_output.txt')

for target in COMMAND_LINE_TARGETS:
    if 'playground' in target:
        for src in PLAYGROUND_SOURCES:
            playground_name = os.path.splitext(os.path.basename(src))[0]
            playground_objects = create_objects(env, [src] + LIB_SOURCES, BUILD_DIR)
            env.Program(target=os.path.join(BUILD_DIR, 'playground', playground_name), source=playground_objects)