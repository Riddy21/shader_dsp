# OpenGL Shader Digital Audio Signal Processor
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Riddy21/shader_dsp)

This project is an audio Digital Signal Processing (DSP) system that uses OpenGL shaders to process audio. 

## About

This project aims to leverage the power of OpenGL shaders to process audio signals. By using the parallel processing capabilities of modern GPUs, we can achieve high performance audio processing. This project is perfect for anyone interested in audio processing, digital signal processing, or computer graphics.

### Inspiration

This project was built from the ground up inspired by the [Teenage Engineering OP-1 Synthesizer](https://teenage.engineering/products/op-1), a portable hardware synthesizer known for its innovative design and unique sound engines. The goal is to implement a software-based version that utilizes the GPU for audio processing, making it both efficient and capable of complex sound manipulation.

### Vision

The primary objectives of this project are:

- Create a fully functional synthesizer using OpenGL/OpenGL ES for audio processing
- Implement a version that runs smoothly on Raspberry Pi hardware
- Provide an open-source alternative to proprietary synthesizer software
- Explore novel approaches to audio synthesis through GPU shaders
- Eventually develop a standalone hardware device running this software

The use of shaders for audio processing opens up possibilities for unique sound design that wouldn't be practical with traditional CPU-based processing, allowing for complex, real-time audio manipulation with minimal latency.

## Dependencies

This project depends on the following libraries:

- [OpenGL Version 3.1](https://www.opengl.org/)
- [Catch2](https://github.com/catchorg/Catch2)
```
sudo apt-get install libsdl2-dev libglew-dev freeglut3-dev g++ scons libportaudio2 mesa-utils xvbf libx11-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-gfx-dev libsdl2-mixer-dev
```

## How to Build

### Docker Setup (Recommended)

This repository includes a Docker setup that provides a consistent development environment with all dependencies pre-installed, plus Claude Code for AI-assisted development.

#### Prerequisites

- [Docker](https://docs.docker.com/get-docker/)
- [Docker Compose](https://docs.docker.com/compose/install/)
- **macOS users**: [XQuartz](https://www.xquartz.org/) for X11 forwarding

#### Quick Start with Makefile (Recommended)

The project now includes a comprehensive Makefile that consolidates all setup and management tasks:

```bash
# Complete setup (PulseAudio + build + container)
make setup

# Complete setup with audio configuration
make all

# Connect to container
make connect

# Clean shutdown
make clean
```

For detailed Makefile usage, see [README-Makefile.md](README-Makefile.md).

#### Quick Start with Script (Legacy)

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
./build/bin/AudioSynthesizer
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

### Native Build

#### Compatible Systems
- Linux (x86_64, ARM)
- Raspberry Pi OS (ARM)

#### Installation
1. Clone the repository to your local machine.
2. Install dependencies:
   ```bash
   sudo apt-get install libsdl2-dev libglew-dev freeglut3-dev g++ scons libportaudio2 mesa-utils xvbf libx11-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-gfx-dev libsdl2-mixer-dev
   ```
3. Build the project:
   ```bash
   scons -j$(nproc)
   ```
4. Run the application:
   ```bash
   ./build/bin/AudioSynthesizer
   ```

#### Raspberry Pi Specific Notes
For Raspberry Pi OS, ensure you have OpenGL drivers enabled:
```bash
sudo raspi-config
```
Navigate to "Advanced Options" > "GL Driver" and select "GL (Fake KMS)".

### Mac and Windows Setup Using Raspberry Pi VM

If you're using macOS or Windows and want to experiment with the Raspberry Pi version, you can set up a virtual machine:

#### Prerequisites
- [VirtualBox](https://www.virtualbox.org/wiki/Downloads) or [VMware](https://www.vmware.com/products/workstation-player.html)
- [Raspberry Pi OS Desktop Image](https://www.raspberrypi.org/software/raspberry-pi-desktop/)

#### Setup Instructions

1. **Install a virtualization platform**:
   - Download and install VirtualBox or VMware on your system

2. **Create a new Raspberry Pi VM**:
   - Open VirtualBox/VMware and create a new virtual machine
   - Select "Linux" as the type and "Debian (32-bit)" as the version
   - Allocate at least 2GB RAM and 20GB disk space
   - Mount the Raspberry Pi OS Desktop ISO file

3. **Install Raspberry Pi OS**:
   - Boot the VM and follow the installation wizard
   - Once installed, restart the VM

4. **Configure OpenGL support**:
   - In VirtualBox: Enable 3D acceleration in VM settings > Display
   - In VMware: Enable 3D acceleration in VM settings > Hardware > Display

5. **Install project dependencies**:
   ```bash
   sudo apt-get update
   sudo apt-get install git
   sudo apt-get install libsdl2-dev libglew-dev freeglut3-dev g++ scons libportaudio2 mesa-utils xvbf libx11-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-gfx-dev libsdl2-mixer-dev
   ```

6. **Clone and build the project**:
   ```bash
   git clone https://github.com/yourusername/shader_dsp.git
   cd shader_dsp
   scons -j$(nproc)
   ./build/bin/AudioSynthesizer
   ```

#### Performance Notes
- VM performance will be slower than native installation
- For best results on non-Linux systems, use the Docker setup described below
- If you experience performance issues, reduce the window size of the application or lower audio buffer settings

## Usage

> :warning: This is currently **WIP**, auto documentation about the API is not yet generated

## Next Steps
- Apply Multitrack support
- Allow dynamic render stage swapping
