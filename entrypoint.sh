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
        elif [[ -n "$HOSTNAME" ]] && echo "$HOSTNAME" | grep -q "docker" 2>/dev/null; then
            # We're in a Docker container, check if ALSA devices actually exist
            if [ ! -d "/dev/snd" ] || [ ! -e "/dev/snd/controlC0" ]; then
                echo "macos"  # Assume macOS if no ALSA devices
            else
                echo "linux"
            fi
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Function to setup audio for Linux
setup_linux_audio() {
    echo "🐧 Linux detected - setting up ALSA"
    
    # Try to detect available audio systems
    if command -v pulseaudio >/dev/null 2>&1; then
        echo "PulseAudio detected - using pulseaudio driver"
        export SDL_AUDIODRIVER=pulseaudio
    elif [ -d "/dev/snd" ] || command -v aplay >/dev/null 2>&1; then
        echo "ALSA detected - setting up ALSA devices"
        setup_alsa_devices
        export SDL_AUDIODRIVER=alsa
        export ALSA_PCM_CARD=0
        export ALSA_PCM_DEVICE=0
    else
        echo "No audio system detected - using dummy driver"
        export SDL_AUDIODRIVER=dummy
    fi
}

# Function to setup audio for macOS
setup_macos_audio() {
    echo "🍎 macOS detected - using dummy audio driver"
    echo "Note: For real audio on macOS, consider using PulseAudio or JACK"
    export SDL_AUDIODRIVER=dummy
}

# Function to setup audio for Windows
setup_windows_audio() {
    echo "🪟 Windows detected - using dummy audio driver"
    echo "Note: For real audio on Windows, consider using DirectSound or WASAPI"
    export SDL_AUDIODRIVER=dummy
}

# Function to setup audio for unknown OS
setup_unknown_audio() {
    echo "❓ Unknown OS detected - using dummy audio driver"
    export SDL_AUDIODRIVER=dummy
}

# Function to setup ALSA devices (Linux only)
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

# Function to test ALSA (Linux only)
test_alsa() {
    if [[ "$(detect_os)" != "linux" ]]; then
        return
    fi
    
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
    
    local os=$(detect_os)
    
    if [[ "$os" == "linux" ]]; then
        # Ensure user is in audio group (Linux only)
        if ! groups | grep -q audio; then
            echo "Adding user to audio group..."
            sudo usermod -a -G audio $USER 2>/dev/null || true
        fi
    fi
    
    # Set video driver based on platform
    if [[ "$os" == "windows" ]]; then
        export SDL_VIDEODRIVER=windows
    else
        export SDL_VIDEODRIVER=x11
    fi
}

# Function to setup platform-specific audio
setup_platform_audio() {
    local os=$(detect_os)
    
    case "$os" in
        "linux")
            setup_linux_audio
            test_alsa
            ;;
        "macos")
            setup_macos_audio
            ;;
        "windows")
            setup_windows_audio
            ;;
        *)
            setup_unknown_audio
            ;;
    esac
}

# Function to display audio driver info
display_audio_info() {
    echo "=== Audio Configuration ==="
    echo "Operating System: $(detect_os)"
    echo "SDL Audio Driver: $SDL_AUDIODRIVER"
    echo "SDL Video Driver: $SDL_VIDEODRIVER"
    
    if [[ "$SDL_AUDIODRIVER" == "alsa" ]]; then
        echo "ALSA Card: $ALSA_PCM_CARD"
        echo "ALSA Device: $ALSA_PCM_DEVICE"
    fi
    echo ""
}

# Main setup sequence
echo "Starting container environment configuration..."

# Setup XDG directories first
setup_xdg_directories

# Setup platform-specific audio
setup_platform_audio

# Setup user permissions
setup_user_permissions

# Display configuration info
display_audio_info

echo "=== Container Setup Complete ==="
echo ""

# Execute the original command or start bash
if [ $# -eq 0 ]; then
    exec /bin/bash
else
    exec "$@"
fi 