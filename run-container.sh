#!/bin/bash
set -e

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
  echo ""
  echo "Example: ./run-container.sh up"
  exit 1
fi

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
  echo "Error: Docker is not running. Please start Docker and try again."
  exit 1
fi

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

COMMAND=$1

case $COMMAND in
  build)
    echo "Building container..."
    docker-compose build
    echo "Container built successfully!"
    ;;
  
  up)
    # Check if running on macOS and setup X11
    if [[ "$OSTYPE" == "darwin"* ]]; then
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
    if ! docker-compose ps | grep -q shader-dsp; then
      echo "Container is not running. Starting it now..."
      docker-compose up -d
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

  rebuild)
    echo "Rebuilding and restarting container..."
    docker-compose down
    docker-compose build
    docker-compose up -d
    echo "Container rebuilt and restarted! Use './run-container.sh connect' to open a shell."
    ;;
    
  *)
    echo "Unknown command: $COMMAND"
    echo "Available commands: up, build, down, connect, rebuild"
    exit 1
    ;;
esac