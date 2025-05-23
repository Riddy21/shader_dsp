# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This project is an OpenGL Shader Digital Audio Signal Processor (DSP) that uses GPU shaders to process audio signals. By leveraging the parallel processing capabilities of modern GPUs, this system aims to achieve high-performance audio processing.

## Build Commands

```bash
# Build the project
scons -j15

# Build with debug symbols
scons --gdb -j15

# Build and run tests
scons tests -j15

# Build and run playground examples
scons playground -j15

# Build documentation
scons docs -j15

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

## UI Architecture

The UI system is built around the following components:

1. **EventLoop** - Singleton that manages the main application loop, dispatches SDL events to event handlers.

2. **EventHandler** - Manages event handler entries that respond to specific events (keyboard, mouse, etc.).

3. **EventHandlerEntry** - Base class for specific event handlers, with subclasses for different event types:
   - KeyboardEventHandlerEntry - Handles keyboard events
   - MouseClickEventHandlerEntry - Handles mouse clicks in specific regions
   - MouseMotionEventHandlerEntry - Handles mouse motion events within a region
   - MouseEnterLeaveEventHandlerEntry - Detects when the mouse enters or leaves a region
   - GlobalMouseUpEventHandlerEntry - Detects mouse button releases when a component is in pressed state

4. **GraphicsDisplay** - Manages window and OpenGL context, renders views.

5. **GraphicsView** - Container for UI components that can be rendered and handle events.
   - DebugView - Shows audio visualization
   - MockInterfaceView - Contains button controls

6. **GraphicsComponent** - Base class for individual UI elements:
   - GraphComponent - For visualizing audio data
   - ButtonComponent - Interactive button for user input

7. **Event Registration Flow**:
   - Views register themselves with the GraphicsDisplay
   - When a view becomes active, it registers its components' event handlers
   - When a view becomes inactive, it unregisters its event handlers
   - This ensures events are only processed by active components

## Implementation Notes

- **Component Access in Views**: Use the `for_each_component` template method in GraphicsView to iterate over components rather than directly accessing the private m_components vector.

- **Event Handler Registration**: Components should implement `register_event_handlers` and `unregister_event_handlers` methods to manage their event handlers. These are called automatically when views are activated/deactivated.

- **Custom Event Handlers**: When creating custom event handlers, extend EventHandlerEntry and implement the `matches` method to define when your handler should be triggered.

- **Render Context**: 
  - Each component has a render context (IRenderableEntity) passed in its constructor 
  - This context is used to ensure OpenGL rendering happens in the correct context before event callbacks are executed
  - For buttons in the mock interface, the AudioRenderer is used as the render context
  - Components should never assume or create their own render context

- **Normalized Coordinates**: The UI uses normalized device coordinates (-1 to 1) for positioning components, which are converted to screen coordinates internally based on the actual window size.

- **Button Event Handling**: Buttons register five types of event handlers:
  1. Mouse button down (when clicking on the button)
  2. Mouse button up (when releasing the click on the button, triggering the callback)
  3. Mouse motion (while hovering over the button)
  4. Mouse enter (when the cursor enters the button area)
  5. Mouse leave (when the cursor leaves the button area)
  6. Global mouse up (to handle when the user clicks on the button but releases outside)