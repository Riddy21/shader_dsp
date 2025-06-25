# Shader DSP Development Environment Makefile
# Consolidates container management, audio setup, and development tasks

.PHONY: help build up down connect rebuild test audio audio-test audio-list pulse-setup pulse-stop clean all setup install-deps

# Default target
help:
	@echo "Shader DSP Development Environment"
	@echo "=================================="
	@echo ""
	@echo "Available targets:"
	@echo "  install-deps - Install and setup Docker and XQuartz dependencies"
	@echo "  setup       - Complete setup (pulse-setup + build + up)"
	@echo "  build       - Build/rebuild the container"
	@echo "  up          - Start the container in detached mode"
	@echo "  down        - Stop and remove the container"
	@echo "  connect     - Connect to the running container shell"
	@echo "  rebuild     - Rebuild and restart the container"
	@echo "  audio       - Setup and test audio (PulseAudio)"
	@echo "  audio-test  - Run audio test script in container"
	@echo "  audio-list  - List audio devices in container"
	@echo "  pulse-setup - Setup PulseAudio server on host (macOS)"
	@echo "  pulse-stop  - Stop PulseAudio server"
	@echo "  clean       - Stop container and PulseAudio, clean up"
	@echo "  all         - Complete setup and audio configuration"
	@echo ""
	@echo "Examples:"
	@echo "  make install-deps - Install Docker and XQuartz dependencies"
	@echo "  make setup  - Complete initial setup"
	@echo "  make all    - Setup everything and configure audio"
	@echo "  make clean  - Clean shutdown"

# Detect operating system
OS := $(shell uname -s)
ifeq ($(OS),Darwin)
	PLATFORM := macos
	DISPLAY := host.docker.internal:0
	SDL_VIDEODRIVER := x11
	ALSA_DEVICES := /dev/null
else ifeq ($(OS),Linux)
	PLATFORM := linux
	DISPLAY := :0
	SDL_VIDEODRIVER := x11
	ALSA_DEVICES := $(shell if [ -d "/dev/snd" ]; then echo "/dev/snd"; else echo "/dev/null"; fi)
else
	PLATFORM := unknown
	DISPLAY := :0
	SDL_VIDEODRIVER := x11
	ALSA_DEVICES := /dev/null
endif

# Export environment variables
export DISPLAY
export SDL_VIDEODRIVER
export ALSA_DEVICES

# PulseAudio status file
PULSE_STATUS_FILE := .pulse_audio_ready

# Check if Docker is running and install if needed
check-docker:
	@echo "Checking Docker installation and status..."
	@if ! command -v docker >/dev/null 2>&1; then \
		echo "Docker not found. Attempting to install..."; \
		if [ "$(PLATFORM)" = "macos" ]; then \
			if command -v brew >/dev/null 2>&1; then \
				echo "Installing Docker via Homebrew..."; \
				brew install --cask docker; \
			else \
				echo "Homebrew not found. Please install Docker manually from https://www.docker.com/products/docker-desktop"; \
				echo "Or install Homebrew first: /bin/bash -c \"\$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""; \
				exit 1; \
			fi; \
		else \
			echo "Please install Docker for your platform from https://www.docker.com/products/docker-desktop"; \
			exit 1; \
		fi; \
	fi
	@if ! docker info >/dev/null 2>&1; then \
		echo "Docker is installed but not running. Starting Docker..."; \
		if [ "$(PLATFORM)" = "macos" ]; then \
			open -a Docker; \
			echo "Waiting for Docker to start..."; \
			sleep 10; \
			for i in 1 2 3 4 5; do \
				if docker info >/dev/null 2>&1; then \
					echo "✓ Docker is now running!"; \
					break; \
				fi; \
				echo "Still waiting for Docker to start... (attempt $$i/5)"; \
				sleep 5; \
			done; \
			if ! docker info >/dev/null 2>&1; then \
				echo "Error: Docker failed to start. Please start Docker manually and try again."; \
				exit 1; \
			fi; \
		else \
			echo "Error: Docker is not running. Please start Docker and try again."; \
			exit 1; \
		fi; \
	fi
	@echo "✓ Docker is running and ready!"

# Setup X11 for macOS - install and open XQuartz if needed
setup-macos-x11:
ifeq ($(PLATFORM),macos)
	@echo "Setting up X11 for macOS..."
	@if ! command -v xquartz >/dev/null 2>&1 && ! ls /Applications/Utilities/XQuartz.app >/dev/null 2>&1; then \
		echo "XQuartz not found. Installing..."; \
		if command -v brew >/dev/null 2>&1; then \
			brew install --cask xquartz; \
			echo "XQuartz installed. Please restart your computer and run 'make setup' again."; \
			echo "After restart, enable 'Allow connections from network clients' in XQuartz preferences."; \
			exit 1; \
		else \
			echo "Homebrew not found. Please install XQuartz manually:"; \
			echo "1. Install Homebrew: /bin/bash -c \"\$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""; \
			echo "2. Install XQuartz: brew install --cask xquartz"; \
			echo "3. Restart your computer"; \
			echo "4. Enable 'Allow connections from network clients' in XQuartz preferences"; \
			exit 1; \
		fi; \
	fi
	@if ! pgrep -x "Xquartz" >/dev/null; then \
		echo "Starting XQuartz..."; \
		open -a XQuartz; \
		echo "Waiting for XQuartz to start..."; \
		sleep 5; \
		for i in 1 2 3 4 5; do \
			if pgrep -x "Xquartz" >/dev/null; then \
				echo "✓ XQuartz is now running!"; \
				break; \
			fi; \
			echo "Still waiting for XQuartz to start... (attempt $$i/5)"; \
			sleep 3; \
		done; \
		if ! pgrep -x "Xquartz" >/dev/null; then \
			echo "Warning: XQuartz may not have started properly. Continuing anyway..."; \
		fi; \
	fi
	@if command -v xhost >/dev/null 2>&1; then \
		xhost +localhost >/dev/null 2>&1 || true; \
		xhost + 127.0.0.1 >/dev/null 2>&1 || true; \
		echo "Enabled X11 connections from localhost and Docker"; \
	fi
	@if [ ! -d "/tmp/.X11-unix" ]; then \
		echo "Creating X11 socket directory..."; \
		sudo mkdir -p /tmp/.X11-unix; \
		sudo chmod 1777 /tmp/.X11-unix; \
	fi
	@echo "✓ X11 setup completed for macOS"
endif

# Setup PulseAudio server on host (macOS) - with status file tracking
pulse-setup:
ifeq ($(PLATFORM),macos)
	@if [ -f "$(PULSE_STATUS_FILE)" ]; then \
		echo "PulseAudio already set up (status file exists)."; \
		echo "If you need to restart PulseAudio, run 'make pulse-stop' first."; \
		exit 0; \
	fi
	@echo "Setting up PulseAudio server on host (macOS)..."
	@if ! command -v pulseaudio >/dev/null 2>&1; then \
		echo "PulseAudio not found. Installing via Homebrew..."; \
		if command -v brew >/dev/null 2>&1; then \
			brew install pulseaudio; \
		else \
			echo "Homebrew not found. Please install Homebrew and rerun this command."; \
			exit 1; \
		fi; \
	else \
		echo "PulseAudio is already installed."; \
	fi
	@echo "Killing any existing PulseAudio processes..."
	@pkill -f pulseaudio 2>/dev/null || true
	@echo "Creating pulse config directory..."
	@mkdir -p ~/.config/pulse
	@echo "Starting PulseAudio as daemon..."
	@pulseaudio -vv --exit-idle-time=-1 \
		--load=module-coreaudio-detect \
		--load=module-native-protocol-tcp \
		--daemonize=yes
	@sleep 2
	@echo "Setting default sink to built-in speakers (sink 1)..."
	@pactl set-default-sink 1 || true
	@echo "✓ PulseAudio server setup complete"
	@echo "PulseAudio setup completed at $(shell date)" > $(PULSE_STATUS_FILE)
	@echo "Status file created: $(PULSE_STATUS_FILE)"
else
	@echo "PulseAudio setup is only needed on macOS"
endif

# Stop PulseAudio server and remove status file
pulse-stop:
ifeq ($(PLATFORM),macos)
	@echo "Stopping PulseAudio server..."
	@pkill -f pulseaudio 2>/dev/null || true
	@rm -f $(PULSE_STATUS_FILE)
	@echo "✓ PulseAudio server stopped and status file removed"
else
	@echo "PulseAudio stop is only needed on macOS"
endif

# Build the container
build: check-docker
	@echo "Building container..."
	@echo "Detected OS: $(PLATFORM)"
	@echo "ALSA devices: $(ALSA_DEVICES)"
	@echo "SDL video driver: $(SDL_VIDEODRIVER)"
	@echo "Display: $(DISPLAY)"
	docker-compose build
	@echo "Container built successfully!"

# Start the container
up: check-docker setup-macos-x11
	@echo "=== Shader DSP Container Launcher ==="
	@echo "Detected OS: $(PLATFORM)"
	@echo "ALSA devices: $(ALSA_DEVICES)"
	@echo "SDL video driver: $(SDL_VIDEODRIVER)"
	@echo "Display: $(DISPLAY)"
	@echo "Starting container in detached mode..."
	docker-compose up -d
	@echo "Container started! Use 'make connect' to open a shell."

# Stop the container
down:
	@echo "Stopping container..."
	docker-compose down
	@echo "Container stopped successfully."

# Connect to container shell (with automatic pulse setup)
connect: check-docker pulse-setup
	@echo "Connecting to container shell..."
	@if ! docker-compose ps | grep -q shader-dsp; then \
		echo "Container is not running. Starting it now..."; \
		$(MAKE) up; \
	fi
	@echo ""
	@echo "NOTE: The connect command needs to be run directly in your terminal, not through Claude."
	@echo "Please run the following command in your terminal to connect to the container:"
	@echo ""
	@echo "cd $(PWD) && docker-compose exec -it shader-dsp bash"
	@echo ""
	@echo "If you're already running this in your terminal and still getting errors,"
	@echo "try using docker directly with: docker exec -it shader_dsp-shader-dsp-1 bash"
	@echo ""
	@docker-compose exec -it shader-dsp bash
	@echo "Disconnected from container shell."

# Rebuild and restart the container
rebuild: down build up
	@echo "Container rebuilt and restarted! Use 'make connect' to open a shell."

# Setup and test audio
audio: check-docker pulse-setup
	@echo "=== Audio Setup and Test ==="
	@if ! docker-compose ps | grep -q shader-dsp; then \
		echo "Container is not running. Starting it now..."; \
		$(MAKE) up; \
	fi
	@echo "Setting up audio..."
	@docker-compose exec -T shader-dsp bash -c "audio_setup.sh"
	@echo ""
	@echo "Audio setup completed. You can now test audio with:"
	@echo "  make audio-test"
	@echo "Or list devices with:"
	@echo "  make audio-list"

# Run audio test script in container
audio-test: check-docker pulse-setup
	@echo "=== Audio Test ==="
	@if ! docker-compose ps | grep -q shader-dsp; then \
		echo "Container is not running. Starting it now..."; \
		$(MAKE) up; \
	fi
	@echo "Running audio test..."
	@docker-compose exec -T shader-dsp bash -c "test_audio.sh"

# List audio devices in container
audio-list: check-docker pulse-setup
	@echo "=== Audio Devices List ==="
	@if ! docker-compose ps | grep -q shader-dsp; then \
		echo "Container is not running. Starting it now..."; \
		$(MAKE) up; \
	fi
	@echo "Listing audio devices..."
	@docker-compose exec -T shader-dsp bash -c "list_devices.sh"

# Complete setup (build + up) - pulse-setup will happen on connect
setup: build up
	@echo "✓ Complete setup finished!"
	@echo "Container is ready. Use 'make connect' to open a shell (PulseAudio will be set up automatically)."

# Complete setup and audio configuration
all: setup audio
	@echo "✓ Complete setup and audio configuration finished!"
	@echo "Everything is ready. Use 'make connect' to open a shell."

# Clean shutdown
clean: down pulse-stop
	@echo "✓ Clean shutdown completed"
	@echo "Container stopped and PulseAudio server stopped"

# Show current status
status:
	@echo "=== Current Status ==="
	@echo "Platform: $(PLATFORM)"
	@echo "Display: $(DISPLAY)"
	@echo "SDL Video Driver: $(SDL_VIDEODRIVER)"
	@echo "ALSA Devices: $(ALSA_DEVICES)"
	@echo ""
	@echo "Container Status:"
	@docker-compose ps 2>/dev/null || echo "Docker Compose not available"
	@echo ""
ifeq ($(PLATFORM),macos)
	@echo "PulseAudio Status:"
	@if [ -f "$(PULSE_STATUS_FILE)" ]; then \
		echo "✓ PulseAudio setup completed (status file exists)"; \
		echo "  Status file: $(PULSE_STATUS_FILE)"; \
		echo "  Created: $(shell cat $(PULSE_STATUS_FILE) 2>/dev/null || echo 'unknown')"; \
	else \
		echo "✗ PulseAudio not set up (no status file)"; \
	fi
	@if pgrep -f pulseaudio >/dev/null; then \
		echo "✓ PulseAudio process is running"; \
	else \
		echo "✗ PulseAudio process is not running"; \
	fi
endif

# Install and setup dependencies (Docker and XQuartz)
install-deps: check-docker setup-macos-x11
	@echo "✓ All dependencies installed and ready!"
	@echo "You can now run 'make setup' to build and start the container." 