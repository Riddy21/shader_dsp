# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This project is an OpenGL Shader Digital Audio Signal Processor (DSP) that uses GPU shaders to process audio signals. By leveraging the parallel processing capabilities of modern GPUs, this system aims to achieve high-performance audio processing.

## Build Commands

```bash
# Build the project
scons

# Build with debug symbols
scons --gdb

# Build and run tests
scons tests

# Build and run playground examples
scons playground

# Build documentation
scons docs
```

## Running the Application

```bash
# Run the main synthesizer
build/bin/AudioSynthesizer
```

## Architecture

The system is organized around these core components:

1. **Audio Renderer** - The central component that manages OpenGL rendering and provides a rendering loop (singleton pattern).

2. **Render Graph** - A directed acyclic graph (DAG) of audio render stages that processes audio data. Handles topological sorting for correct rendering order.

3. **Render Stages** - Individual processing nodes in the graph:
   - Generator stages (sine, sawtooth, file input, etc.)
   - Effect stages (echo, filters, gain, etc.)
   - Routing stages (multitrack join, etc.)
   - Output/final stages

4. **Parameters** - Audio parameters that can be passed between render stages:
   - Uniform parameters
   - Texture parameters
   - Buffer parameters

5. **Audio Synthesizer** - High-level interface (singleton) for audio generation, track management and playback.

6. **Audio Output** - Sends processed audio to output destinations (files, audio devices).

## Core Data Flow

1. Audio data is processed through a render graph of shader-based render stages
2. Each render stage processes audio data using OpenGL fragment shaders
3. Audio data is passed between stages using OpenGL textures
4. Final output is rendered to a framebuffer and then read back to the CPU
5. Audio is then sent to the output system (file, audio device)

## Dependencies

- OpenGL 3.1+
- SDL2 (and related SDL libraries)
- GLEW
- Catch2 (for testing)
- pthread
- X11

## Test Framework

The project uses Catch2 for unit testing. Tests are written in the `tests` directory and can be executed individually.

```bash
# Build and run a specific test
scons tests/audio_buffer_test
```

## Shader System

Audio processing is primarily done through GLSL shaders in the `shaders` directory:
- Generator shaders (sine, sawtooth, etc.) in `shaders/generator_render_stages/`
- Effect shaders (echo, filter, gain, etc.) in `shaders/effect_render_stages/`
- Routing shaders in `shaders/routing_render_stages/`
- Global settings in `shaders/settings/`