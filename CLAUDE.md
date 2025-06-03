# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This project is an OpenGL Shader Digital Audio Signal Processor (DSP) that uses GPU shaders to process audio signals. By leveraging the parallel processing capabilities of modern GPUs, this system aims to achieve high-performance audio processing. It's inspired by the Teenage Engineering OP-1 Synthesizer and aims to provide similar functionality through software.

## Build Commands

```bash
# Build the project (using multiple cores)
scons -j$(nproc)

# Build with debug symbols
scons --gdb -j$(nproc)

# Build and run all tests
scons tests -j$(nproc)

# Build and run a specific test
scons tests/audio_buffer_test

# Build and run playground examples
scons playground -j$(nproc)

# Build a specific playground example
scons playground/audio_generator_test

# Build documentation
scons docs -j$(nproc)
```

## Running the Application

```bash
# Run the main synthesizer
./build/bin/AudioSynthesizer
```

## Docker Environment

For consistent development across platforms, a Docker environment is provided:

```bash
# Run the Docker environment
./run-container.sh

# Or manually:
docker-compose up -d
docker-compose exec shader-dsp zsh
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

7. **Event System** - Handles user inputs and interface interactions:
   - EventLoop - Main application loop and event dispatcher
   - EventHandler - Manages event handlers for different input types
   - EventHandlerEntries - Specific event handler implementations

8. **UI System** - Provides graphical interface for controlling the synthesizer:
   - GraphicsDisplay - Window and rendering manager
   - GraphicsViews - Containers for UI components
   - GraphicsComponents - Individual UI elements (buttons, graphs, etc.)

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

These can be installed on Ubuntu/Debian with:
```bash
sudo apt-get install libsdl2-dev libglew-dev freeglut3-dev g++ scons libportaudio2 mesa-utils xvbf libx11-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-gfx-dev libsdl2-mixer-dev
```

## Test Framework

The project uses Catch2 for unit testing. Tests are written in the `tests` directory and can be executed individually.

```bash
# Build and run a specific test
scons tests/audio_buffer_test
```

Tests run with xvfb-run to provide a virtual framebuffer for OpenGL operations during testing.

## Shader System

Audio processing is primarily done through GLSL shaders in the `shaders` directory:
- Generator shaders (sine, sawtooth, etc.) in `shaders/generator_render_stages/`
- Effect shaders (echo, filter, gain, etc.) in `shaders/effect_render_stages/`
- Routing shaders in `shaders/routing_render_stages/`
- Global settings in `shaders/settings/`

Custom audio effects and generators can be implemented by creating new shader files in the appropriate directories.

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
   - MockInterfaceView - Contains button controls for playback and effects

6. **GraphicsComponent** - Base class for individual UI elements:
   - Uses unique_ptr for child component ownership
   - Supports component composition with parent-child relationships
   - Automatically manages memory for child components
   - Provides methods for adding, removing, and accessing child components

7. **UI Component Types**:
   - ButtonComponent - Base interactive button with hover/press states
   - TextComponent - Renders text with custom fonts, sizes, and colors
   - ImageComponent - Displays images with scale modes and tint colors
   - TextButtonComponent - Button with text rendering
   - ImageButtonComponent - Button with image rendering
   
8. **Event Registration Flow**:
   - Views register themselves with the GraphicsDisplay
   - When a view becomes active, it registers its components' event handlers
   - When a view becomes inactive, it unregisters its event handlers
   - This ensures events are only processed by active components

## Component System Implementation

The UI uses a component-based architecture with clear ownership semantics:

1. **Component Hierarchy**:
   - Each component can have multiple child components
   - Parent components own their children through unique_ptr
   - Children are automatically deleted when their parent is destroyed
   - Components render themselves and then all their children

2. **Component Creation and Ownership**:
   ```cpp
   // Create a component with 'new' and pass ownership to the parent
   auto child = new TextComponent(x, y, width, height, "Text");
   parent->add_child(child); // Parent takes ownership
   ```

3. **Specialized Components**:
   - TextComponent: For text rendering with multiple font support
   - ImageComponent: For displaying images with various scale modes
   - TextButtonComponent: Button with text label that responds to state changes
   - ImageButtonComponent: Button with image that responds to state changes

4. **Font Management**:
   - Global font registry with name-based access
   - Automatic font loading from files or system fonts
   - Font caching for different sizes
   - Default font fallback mechanism

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

- **Component Composition Pattern**:
  - When creating composite components, use the add_child method to establish parent-child relationships
  - Store raw pointers to child components for direct access (the parent owns the actual memory)
  - Use the update_children method to propagate state changes to child components

## Current Development

The project is currently implementing features for controlling audio effects and generators through the UI. This includes:
1. Play/pause controls for audio playback
2. Buttons for applying different effects (echo, frequency filter)
3. Foundation for more comprehensive controls for audio manipulation