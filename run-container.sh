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

COMMAND=$1

case $COMMAND in
  build)
    echo "Building container..."
    docker-compose build
    echo "Container built successfully!"
    ;;
  
  up)
    # Check if running on macOS and provide X11 setup instructions
    if [[ "$OSTYPE" == "darwin"* ]]; then
      echo "Detected macOS - Setting up X11 forwarding..."
      
      # Check if XQuartz is installed
      if ! command -v xquartz &> /dev/null && ! ls /Applications/Utilities/XQuartz.app &> /dev/null 2>&1; then
        echo "⚠️  XQuartz not found. Please install it with:"
        echo "   brew install --cask xquartz"
        echo "   Then restart and enable 'Allow connections from network clients' in XQuartz preferences."
      fi
      
      # Set up DISPLAY for macOS
      if [ -z "$DISPLAY" ]; then
        export DISPLAY=host.docker.internal:0
        echo "Set DISPLAY to host.docker.internal:0"
      fi
      
      # Allow X11 connections (requires XQuartz to be running)
      if command -v xhost &> /dev/null; then
        xhost +localhost > /dev/null 2>&1 || true
        echo "Enabled X11 connections from localhost"
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
    echo "cd $(pwd) && docker-compose exec -it shader-dsp zsh"
    echo ""
    echo "If you're already running this in your terminal and still getting errors,"
    echo "try using docker directly with: docker exec -it shader_dsp-shader-dsp-1 zsh"
    echo ""
    
    # Still try to connect, which might work if this is run from a real terminal
    docker-compose exec -it shader-dsp zsh
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