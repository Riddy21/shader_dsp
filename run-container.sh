#!/bin/bash
set -e

# Detect operating system
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$(uname)" == "Linux" ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]] || [[ "$(uname)" == "Darwin" ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Setup environment variables based on OS
setup_environment() {
    local os=$(detect_os)
    
    echo "Detected OS: $os"
    
    case "$os" in
        "linux")
            # Check if ALSA devices exist
            if [ -d "/dev/snd" ]; then
                export ALSA_DEVICES="/dev/snd"
                echo "ALSA devices found at /dev/snd"
            else
                export ALSA_DEVICES="/dev/null"
                echo "No ALSA devices found, using dummy audio"
            fi
            export SDL_VIDEODRIVER="x11"
            ;;
        "macos")
            export ALSA_DEVICES="/dev/null"
            export SDL_VIDEODRIVER="x11"
            export DISPLAY="host.docker.internal:0"
            echo "macOS detected - using X11 forwarding"
            ;;
        "windows")
            export ALSA_DEVICES="/dev/null"
            export SDL_VIDEODRIVER="windows"
            echo "Windows detected - using Windows video driver"
            ;;
        *)
            export ALSA_DEVICES="/dev/null"
            export SDL_VIDEODRIVER="x11"
            echo "Unknown OS - using default settings"
            ;;
    esac
}

# Function to setup X11 for macOS
setup_macos_x11() {
    echo "Setting up X11 for macOS..."
    
    # Check if XQuartz is installed
    if ! command -v xquartz &> /dev/null && ! ls /Applications/Utilities/XQuartz.app &> /dev/null 2>&1; then
        echo "⚠️  XQuartz not found. Please install it with:"
        echo "   brew install --cask xquartz"
        echo "   Then restart and enable 'Allow connections from network clients' in XQuartz preferences."
        echo "   After installation, run this script again."
        return 1
    fi
    
    # Start XQuartz if not running
    if ! pgrep -x "Xquartz" > /dev/null; then
        echo "Starting XQuartz..."
        open -a XQuartz
        sleep 3  # Give XQuartz time to start
    fi
    
    # Set up DISPLAY for macOS
    if [ -z "$DISPLAY" ]; then
        export DISPLAY=host.docker.internal:0
        echo "Set DISPLAY to host.docker.internal:0"
    fi
    
    # Allow X11 connections from Docker
    if command -v xhost &> /dev/null; then
        xhost +localhost > /dev/null 2>&1 || true
        xhost + 127.0.0.1 > /dev/null 2>&1 || true
        echo "Enabled X11 connections from localhost and Docker"
    fi
    
    # Ensure X11 socket directory exists and has correct permissions
    if [ ! -d "/tmp/.X11-unix" ]; then
        echo "Creating X11 socket directory..."
        sudo mkdir -p /tmp/.X11-unix
        sudo chmod 1777 /tmp/.X11-unix
    fi
    
    echo "X11 setup completed for macOS"
    return 0
}

# Check if Docker is running
check_docker() {
    if ! docker info >/dev/null 2>&1; then
        echo "Error: Docker is not running. Please start Docker and try again."
        exit 1
    fi
}

# Default command if none provided
if [ $# -eq 0 ]; then
    echo "Usage: ./run-container.sh [command]"
    echo ""
    echo "Commands:"
    echo "  up      - Start the container in detached mode"
    echo "  build   - Build/rebuild the container"
    echo "  down    - Stop and remove the container"
    echo "  connect - Connect to the running container shell"
    echo "  rebuild - Rebuild and restart the container (down+build+up)"
    echo "  test    - Test the AudioSynthesizer application"
    echo ""
    echo "Example: ./run-container.sh up"
    exit 1
fi

COMMAND=$1

case $COMMAND in
    build)
        echo "Building container..."
        setup_environment
        check_docker
        docker-compose build
        echo "Container built successfully!"
        ;;
    
    up)
        echo "=== Shader DSP Container Launcher ==="
        setup_environment
        
        # Check if running on macOS and setup X11
        if [[ "$(detect_os)" == "macos" ]]; then
            echo "Detected macOS - Setting up X11 forwarding..."
            if ! setup_macos_x11; then
                echo "X11 setup failed. Please install XQuartz and try again."
                exit 1
            fi
        else
            # For Linux, just set DISPLAY if not already set
            if [ -z "$DISPLAY" ]; then
                export DISPLAY=:0
                echo "Set DISPLAY to :0 for Linux"
            fi
        fi
        
        check_docker
        echo "Starting container in detached mode..."
        docker-compose up -d
        echo "Container started! Use './run-container.sh connect' to open a shell."
        ;;
    
    down)
        echo "Stopping container..."
        docker-compose down
        echo "Container stopped successfully."
        ;;
    
    connect)
        echo "Connecting to container shell..."
        check_docker
        
        if ! docker-compose ps | grep -q shader-dsp; then
            echo "Container is not running. Starting it now..."
            ./run-container.sh up
        fi
        
        # Print instructions for running the connect command directly from a terminal
        echo ""
        echo "NOTE: The connect command needs to be run directly in your terminal, not through Claude."
        echo "Please run the following command in your terminal to connect to the container:"
        echo ""
        echo "cd $(pwd) && docker-compose exec -it shader-dsp bash"
        echo ""
        echo "If you're already running this in your terminal and still getting errors,"
        echo "try using docker directly with: docker exec -it shader_dsp-shader-dsp-1 bash"
        echo ""
        
        # Still try to connect, which might work if this is run from a real terminal
        docker-compose exec -it shader-dsp bash
        echo "Disconnected from container shell."
        ;;

    test)
        echo "Testing AudioSynthesizer application..."
        check_docker
        
        if ! docker-compose ps | grep -q shader-dsp; then
            echo "Container is not running. Starting it now..."
            ./run-container.sh up
        fi
        
        echo "Running AudioSynthesizer..."
        docker-compose exec -T shader-dsp bash -c "cd /workspace && ./build/bin/AudioSynthesizer 2>&1"
        ;;
    
    rebuild)
        echo "Rebuilding and restarting container..."
        setup_environment
        check_docker
        docker-compose down
        docker-compose build
        docker-compose up -d
        echo "Container rebuilt and restarted! Use './run-container.sh connect' to open a shell."
        ;;
        
    *)
        echo "Unknown command: $COMMAND"
        echo "Available commands: up, build, down, connect, rebuild, test"
        exit 1
        ;;
esac