#!/bin/bash
set -e

echo "=== Container Environment Setup ==="

# Function to setup XDG runtime directories
setup_xdg_directories() {
    echo "Setting up XDG runtime directories..."
    
    # Create XDG directories if they don't exist
    mkdir -p /tmp/runtime-root /tmp/data /tmp/config /tmp/cache 2>/dev/null || true
    
    # Set proper permissions
    chmod 700 /tmp/runtime-root 2>/dev/null || true
    chmod 755 /tmp/data /tmp/config /tmp/cache 2>/dev/null || true
    
    # Set XDG environment variables
    export XDG_RUNTIME_DIR=/tmp/runtime-root
    export XDG_DATA_HOME=/tmp/data
    export XDG_CONFIG_HOME=/tmp/config
    export XDG_CACHE_HOME=/tmp/cache
    
    echo "XDG directories setup complete"
}

# Function to detect operating system
detect_os() {
    # Check if we're running in a container on macOS host
    if [[ "$OSTYPE" == "darwin"* ]] || [[ "$(uname)" == "Darwin" ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$(uname)" == "Linux" ]]; then
        # Check if we're in a container on macOS host
        if [[ -f "/proc/version" ]] && grep -q "Darwin" /proc/version 2>/dev/null; then
            echo "macos"
        elif [[ -n "$DOCKER_HOST" ]] && echo "$DOCKER_HOST" | grep -q "darwin" 2>/dev/null; then
            echo "macos"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Function to setup video driver
setup_video_driver() {
    echo "Setting up video driver..."
    
    local os=$(detect_os)
    
    # Set video driver based on platform
    if [[ "$os" == "windows" ]]; then
        export SDL_VIDEODRIVER=windows
    else
        export SDL_VIDEODRIVER=x11
    fi
    
    echo "Video driver setup complete"
}

# Function to display configuration info
display_config_info() {
    echo "=== Configuration ==="
    echo "Operating System: $(detect_os)"
    echo "SDL Video Driver: $SDL_VIDEODRIVER"
    echo ""
}

# Main setup sequence
echo "Starting container environment configuration..."

# Setup XDG directories first
setup_xdg_directories

# Setup video driver
setup_video_driver

# Display configuration info
display_config_info

echo "=== Container Setup Complete ==="
echo ""

# Execute the original command or start bash
if [ $# -eq 0 ]; then
    exec /bin/bash
else
    exec "$@"
fi 