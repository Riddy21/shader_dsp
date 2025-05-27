# OpenGL Shader Digital Audio Signal Processor
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Riddy21/shader_dsp)

This project is an audio Digital Signal Processing (DSP) system that uses OpenGL shaders to process audio. 

## About

This project aims to leverage the power of OpenGL shaders to process audio signals. By using the parallel processing capabilities of modern GPUs, we can achieve high performance audio processing. This project is perfect for anyone interested in audio processing, digital signal processing, or computer graphics.

## Dependencies

This project depends on the following libraries:

- [OpenGL Version 3.1](https://www.opengl.org/)
- [Catch2](https://github.com/catchorg/Catch2)
```
sudo apt-get install libsdl2-dev libglew-dev freeglut3-dev g++ scons libportaudio2 mesa-utils xvbf libx11-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-gfx-dev libsdl2-mixer-dev
```

## How to Build

### Native Build

1. Clone the repository to your local machine.
2. `scons`
3. `./build/bin/main`

### Docker Setup (Recommended)

This repository includes a Docker setup that provides a consistent development environment with all dependencies pre-installed, plus Claude Code for AI-assisted development.

#### Prerequisites

- [Docker](https://docs.docker.com/get-docker/)
- [Docker Compose](https://docs.docker.com/compose/install/)

#### Quick Start

1. Run the provided script to build and start the container:

```bash
./run-container.sh
```

This script will:
- Build the Docker image with all necessary dependencies
- Start the container
- Connect you to the container's shell

2. Inside the container, you can build and run the project:

```bash
scons
./build/bin/main
```

3. To exit the container, type `exit` or press `Ctrl+D`.

#### Manual Docker Setup

If you prefer to run the Docker commands manually:

1. Build and start the container:

```bash
docker-compose up -d
```

2. Connect to the container:

```bash
docker-compose exec shader-dsp zsh
```

3. When finished, you can stop the container:

```bash
docker-compose down
```

#### Using Claude Code in the Container

The container comes with Claude Code pre-installed to help with development. To use it:

1. Inside the container, run:

```bash
claude
```

2. Follow the authentication instructions.

3. Once authenticated, you can use Claude Code for various tasks:

```bash
claude explain [file path]
claude fix [issue description]
claude add [feature description]
claude help
```

## Usage

> :warning: This is currently **WIP**, auto documentation about the API is not yet generated

## Next Steps
- Apply Multitrack support
- Allow dynamic render stage swapping
