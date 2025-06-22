#!/bin/bash
set -e

echo "=== Container Environment Setup ==="

# Function to setup XDG runtime directories
setup_xdg_directories() {
    echo "Setting up XDG runtime directories..."
    
    # Create XDG directories if they don't exist
    mkdir -p /tmp/runtime-developer /tmp/data /tmp/config /tmp/cache 2>/dev/null || true
    
    # Set proper permissions
    chmod 700 /tmp/runtime-developer 2>/dev/null || true
    chmod 755 /tmp/data /tmp/config /tmp/cache 2>/dev/null || true
    
    # Set XDG environment variables
    export XDG_RUNTIME_DIR=/tmp/runtime-developer
    export XDG_DATA_HOME=/tmp/data
    export XDG_CONFIG_HOME=/tmp/config
    export XDG_CACHE_HOME=/tmp/cache
    
    echo "XDG directories setup complete"
}

# Function to setup ALSA devices
setup_alsa_devices() {
    echo "Setting up ALSA devices..."
    
    # Create ALSA device directory if it doesn't exist
    sudo mkdir -p /dev/snd 2>/dev/null || true
    
    # Create ALSA device nodes if they don't exist (with sudo)
    if [ ! -e /dev/snd/controlC0 ]; then
        echo "Creating control device..."
        sudo mknod /dev/snd/controlC0 c 116 0 2>/dev/null || true
    fi
    
    if [ ! -e /dev/snd/pcmC0D0p ]; then
        echo "Creating playback device..."
        sudo mknod /dev/snd/pcmC0D0p c 116 16 2>/dev/null || true
    fi
    
    if [ ! -e /dev/snd/pcmC0D0c ]; then
        echo "Creating capture device..."
        sudo mknod /dev/snd/pcmC0D0c c 116 24 2>/dev/null || true
    fi
    
    if [ ! -e /dev/snd/timer ]; then
        echo "Creating timer device..."
        sudo mknod /dev/snd/timer c 116 33 2>/dev/null || true
    fi
    
    # Set permissions (with sudo)
    sudo chmod 666 /dev/snd/* 2>/dev/null || true
    sudo chown root:audio /dev/snd/* 2>/dev/null || true
    echo "ALSA devices setup complete"
}

# Function to test ALSA
test_alsa() {
    echo "Testing ALSA configuration..."
    
    if command -v aplay >/dev/null 2>&1; then
        echo "ALSA playback devices:"
        aplay -l 2>/dev/null || echo "No playback devices found"
        
        echo "ALSA capture devices:"
        arecord -l 2>/dev/null || echo "No capture devices found"
    else
        echo "ALSA utilities not available"
    fi
    
    if command -v amixer >/dev/null 2>&1; then
        echo "ALSA mixer controls:"
        amixer scontrols 2>/dev/null || echo "No mixer controls found"
    fi
}

# Function to setup user permissions
setup_user_permissions() {
    echo "Setting up user permissions..."
    
    # Ensure user is in audio group
    if ! groups | grep -q audio; then
        echo "Adding user to audio group..."
        sudo usermod -a -G audio $USER 2>/dev/null || true
    fi
    
    # Set ALSA environment variables
    export ALSA_PCM_CARD=0
    export ALSA_PCM_DEVICE=0
    export SDL_AUDIODRIVER=alsa
    export SDL_VIDEODRIVER=x11
}

# Function to check if we're on macOS
check_macos() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "⚠️  Detected macOS - ALSA may not work properly"
        echo "   On macOS, consider using PulseAudio or JACK instead"
        echo "   ALSA is primarily for Linux systems"
        return 1
    fi
    return 0
}

# Main setup sequence
echo "Starting container environment configuration..."

# Setup XDG directories first
setup_xdg_directories

# Check if we're on macOS
check_macos

# Setup devices
setup_alsa_devices

# Setup user permissions
setup_user_permissions

# Test ALSA
test_alsa

echo "=== Container Setup Complete ==="
echo ""

# Execute the original command or start bash
if [ $# -eq 0 ]; then
    exec /bin/bash
else
    exec "$@"
fi 