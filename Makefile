# Shader DSP Development Environment Makefile
# Simplified container management and development tasks

.PHONY: help build up down connect clean status

# Default target
help:
	@echo "Shader DSP Development Environment"
	@echo "=================================="
	@echo ""
	@echo "Available targets:"
	@echo "  build   - Complete setup (install dependencies, build container)"
	@echo "  up      - Start XQuartz, PulseAudio server, and Docker container"
	@echo "  down    - Stop container, PulseAudio, and XQuartz"
	@echo "  connect - Connect to the running container shell"
	@echo "  clean   - Complete cleanup (down + remove containers/images)"
	@echo "  status  - Show current status of all components"
	@echo ""
	@echo "Simple Workflow:"
	@echo "  1. First time: make build"
	@echo "  2. Daily use:  make up → make connect → work → make down"
	@echo "  3. Cleanup:    make clean"
	@echo ""
	@echo "Examples:"
	@echo "  make build   - Complete initial setup"
	@echo "  make up      - Start everything"
	@echo "  make connect - Open container shell"
	@echo "  make down    - Stop everything"
	@echo "  make clean   - Complete cleanup"
	@echo ""
	@echo "The Makefile automatically handles:"
	@echo "  • Installing Docker and Homebrew (if needed)"
	@echo "  • Setting up X11/XQuartz for macOS"
	@echo "  • Installing and configuring PulseAudio for audio support"
	@echo "  • Building and managing the Docker container"

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

# Target files for tracking completion
BUILD_DIR := build/env
DOCKER_SETUP_FILE := $(BUILD_DIR)/docker-setup.done
X11_SETUP_FILE := $(BUILD_DIR)/x11-setup.done
X11_STARTED_FILE := $(BUILD_DIR)/x11-started.done
PULSE_SETUP_FILE := $(BUILD_DIR)/pulse-setup.done
PULSE_STARTED_FILE := $(BUILD_DIR)/pulse-started.done
CONTAINER_BUILT_FILE := $(BUILD_DIR)/container-built.done
CONTAINER_RUNNING_FILE := $(BUILD_DIR)/container-running.done

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Complete setup - install dependencies and build container
build: $(DOCKER_SETUP_FILE) $(X11_SETUP_FILE) $(PULSE_SETUP_FILE) $(CONTAINER_BUILT_FILE)
	@echo "✓ Complete setup finished!"
	@echo "Run 'make up' to start everything"

# Setup Docker
$(DOCKER_SETUP_FILE):
	@mkdir -p $(BUILD_DIR)
	@echo "Setting up Docker..."
	@echo "Checking Docker installation and status..."
	@set -e; \
	if ! command -v docker >/dev/null 2>&1; then \
		echo "Docker not found. Installing..."; \
		if [ "$(PLATFORM)" = "macos" ]; then \
			if command -v brew >/dev/null 2>&1; then \
				echo "Installing Docker via Homebrew..."; \
				brew install --cask docker; \
			else \
				echo "Homebrew not found. Installing Homebrew first..."; \
				/bin/bash -c "$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"; \
				echo "Installing Docker via Homebrew..."; \
				brew install --cask docker; \
			fi; \
		else \
			echo "Please install Docker for your platform from https://www.docker.com/products/docker-desktop"; \
			exit 1; \
		fi; \
	fi
	@set -e; \
	if ! docker info >/dev/null 2>&1; then \
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
	@touch $@

# Setup X11 for macOS
$(X11_SETUP_FILE):
	@mkdir -p $(BUILD_DIR)
	@echo "Setting up X11..."
ifeq ($(PLATFORM),macos)
	@echo "Setting up X11 for macOS..."
	@set -e; \
	if ! command -v xquartz >/dev/null 2>&1 && ! ls /Applications/Utilities/XQuartz.app >/dev/null 2>&1; then \
		echo "XQuartz not found. Installing..."; \
		if command -v brew >/dev/null 2>&1; then \
			brew install --cask xquartz; \
			echo "XQuartz installed. Please restart your computer and run 'make build' again."; \
			echo "After restart, enable 'Allow connections from network clients' in XQuartz preferences."; \
			exit 1; \
		else \
			echo "Homebrew not found. Please install XQuartz manually:"; \
			echo "1. Install Homebrew: /bin/bash -c \"$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""; \
			echo "2. Install XQuartz: brew install --cask xquartz"; \
			echo "3. Restart your computer"; \
			echo "4. Enable 'Allow connections from network clients' in XQuartz preferences"; \
			exit 1; \
		fi; \
	fi
	@set -e; \
	if [ ! -d "/tmp/.X11-unix" ]; then \
		echo "Creating X11 socket directory..."; \
		sudo mkdir -p /tmp/.X11-unix; \
		sudo chmod 1777 /tmp/.X11-unix; \
	fi
	@echo "✓ X11 setup completed for macOS"
else
	@echo "X11 setup not needed on $(PLATFORM)"
endif
	@touch $@

# Setup PulseAudio
$(PULSE_SETUP_FILE):
	@mkdir -p $(BUILD_DIR)
	@echo "Setting up PulseAudio..."
ifeq ($(PLATFORM),macos)
	@echo "Setting up PulseAudio..."
	@set -e; \
	if ! command -v pulseaudio >/dev/null 2>&1; then \
		echo "PulseAudio not found. Installing via Homebrew..."; \
		if command -v brew >/dev/null 2>&1; then \
			brew install pulseaudio; \
		else \
			echo "Homebrew not found. Installing Homebrew first..."; \
			/bin/bash -c "$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"; \
			echo "Installing PulseAudio via Homebrew..."; \
			brew install pulseaudio; \
		fi; \
	else \
		echo "✓ PulseAudio is already installed."; \
	fi
	@echo "✓ PulseAudio installation completed"
else
	@echo "PulseAudio installation is only needed on macOS"
endif
	@touch $@

# Build the container
$(CONTAINER_BUILT_FILE): $(DOCKER_SETUP_FILE) $(X11_SETUP_FILE) $(PULSE_SETUP_FILE) docker-compose.yml Dockerfile
	@echo "Building container..."
	@echo "ALSA devices: $(ALSA_DEVICES)"
	@echo "SDL video driver: $(SDL_VIDEODRIVER)"
	@echo "Display: $(DISPLAY)"
	@set -e; docker-compose build
	@echo "✓ Container built successfully!"
	@touch $@

# Start XQuartz and Docker container
up: $(CONTAINER_RUNNING_FILE)
	@echo ""
	@echo "✓ Everything started successfully!"
	@echo "Use 'make connect' to open a shell in the container."

# Start X11
$(X11_STARTED_FILE): $(X11_SETUP_FILE)
ifeq ($(PLATFORM),macos)
	@set -e; \
	if ! pgrep -x "Xquartz" >/dev/null; then \
		echo "Starting XQuartz..."; \
		open -a XQuartz; \
		echo "Waiting for XQuartz to start and initialize..."; \
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
			echo "Error: XQuartz failed to start properly."; \
			exit 1; \
		fi; \
	fi
	@set -e; \
	if command -v xhost >/dev/null 2>&1; then \
		echo "Enabling X11 connections..."; \
		DISPLAY=:0 xhost +localhost; \
		DISPLAY=:0 xhost + 127.0.0.1; \
		echo "✓ X11 connection setup completed"; \
	fi
else
	@echo "X11 startup not needed on $(PLATFORM)"
endif
	@touch $@

# Start PulseAudio server on host (macOS)
$(PULSE_STARTED_FILE): $(PULSE_SETUP_FILE)
	@echo "Starting PulseAudio..."
ifeq ($(PLATFORM),macos)
	@echo "Starting PulseAudio server on host (macOS)..."
	@set -e; \
	echo "Killing any existing PulseAudio processes..."; \
	pkill -f pulseaudio 2>/dev/null || true; \
	echo "Creating pulse config directory..."; \
	mkdir -p ~/.config/pulse; \
	echo "Copying PulseAudio client configuration..."; \
	cp conf/pulse-client.conf ~/.config/pulse/client.conf; \
	echo "Starting PulseAudio as daemon..."; \
	pulseaudio -vv --exit-idle-time=-1 \
		--load=module-coreaudio-detect \
		--load=module-native-protocol-tcp \
		--daemonize=yes; \
	sleep 2; \
	echo "✓ PulseAudio server started successfully"; \
	if [ -f ~/.config/pulse/cookie ]; then \
		echo "✓ PulseAudio cookie exists in home directory"; \
	else \
		echo "Error: PulseAudio cookie missing from home directory. Audio in container may not work."; \
		exit 1; \
	fi
else
	@echo "PulseAudio setup is only needed on macOS"
endif
	@touch $@

# Start container
$(CONTAINER_RUNNING_FILE): $(CONTAINER_BUILT_FILE) $(PULSE_STARTED_FILE) $(X11_STARTED_FILE)
	@echo "Starting Docker container..."
	@set -e; docker-compose up -d
	@touch $@

# Connect to container shell
connect: $(CONTAINER_RUNNING_FILE)
	@echo "Connecting to container shell..."
	@echo ""
	@echo "NOTE: The connect command needs to be run directly in your terminal, not through Claude."
	@echo "Please run the following command in your terminal to connect to the container:"
	@echo ""
	@echo "cd $(PWD) && docker-compose exec -it shader-dsp bash"
	@echo ""
	@echo "If you're already running this in your terminal and still getting errors,"
	@echo "try using docker directly with: docker exec -it shader_dsp-shader-dsp-1 bash"
	@echo ""
	@set -e; docker-compose exec -it shader-dsp bash
	@echo "Disconnected from container shell."

# Stop container, PulseAudio, and XQuartz
down:
	@echo "=== Stopping Everything ==="
	@echo "Step 1: Stopping Docker container..."
	@set -e; docker-compose down
	@echo "✓ Container stopped"
	@echo ""
ifeq ($(PLATFORM),macos)
	@echo "Step 2: Stopping PulseAudio server..."
	@set -e; pkill -f pulseaudio 2>/dev/null || true
	@echo "✓ PulseAudio server stopped"
	@echo ""
	@echo "Step 3: Stopping XQuartz..."
	@set -e; pkill -x "Xquartz" 2>/dev/null || true
	@echo "✓ XQuartz stopped"
endif
	@echo ""
	@echo "✓ Everything stopped successfully!"
	@rm -f $(CONTAINER_RUNNING_FILE) $(PULSE_STARTED_FILE) $(X11_STARTED_FILE)

# Complete cleanup
clean: down
	@echo "=== Complete Cleanup ==="
	@echo "Removing containers and images..."
	@set -e; docker-compose down --rmi all --volumes --remove-orphans
	@echo "✓ Complete cleanup finished"
	@rm -rf $(BUILD_DIR)

# Show current status
status:
	@echo "=== Current Status ==="
	@echo "Platform: $(PLATFORM)"
	@echo "Display: $(DISPLAY)"
	@echo "SDL Video Driver: $(SDL_VIDEODRIVER)"
	@echo "ALSA Devices: $(ALSA_DEVICES)"
	@echo ""
	@echo "Build Status:"
	@if [ -f $(DOCKER_SETUP_FILE) ]; then echo "✓ Docker setup completed"; else echo "✗ Docker setup not completed"; fi
	@if [ -f $(X11_SETUP_FILE) ]; then echo "✓ X11 setup completed"; else echo "✗ X11 setup not completed"; fi
	@if [ -f $(PULSE_SETUP_FILE) ]; then echo "✓ PulseAudio setup completed"; else echo "✗ PulseAudio setup not completed"; fi
	@if [ -f $(CONTAINER_BUILT_FILE) ]; then echo "✓ Container built"; else echo "✗ Container not built"; fi
	@if [ -f $(CONTAINER_RUNNING_FILE) ]; then echo "✓ Container running"; else echo "✗ Container not running"; fi
	@if [ -f $(PULSE_STARTED_FILE) ]; then echo "✓ PulseAudio started"; else echo "✗ PulseAudio not started"; fi
	@if [ -f $(X11_STARTED_FILE) ]; then echo "✓ X11 started"; else echo "✗ X11 not started"; fi
	@echo ""
	@echo "Container Status:"
	@docker-compose ps 2>/dev/null || echo "Docker Compose not available"
	@echo ""
ifeq ($(PLATFORM),macos)
	@echo "XQuartz Status:"
	@if pgrep -x "Xquartz" >/dev/null; then \
		echo "✓ XQuartz is running"; \
	else \
		echo "✗ XQuartz is not running"; \
	fi
	@echo ""
	@echo "PulseAudio Status:"
	@if command -v pulseaudio >/dev/null 2>&1; then \
		echo "✓ PulseAudio is installed"; \
	else \
		echo "✗ PulseAudio is not installed"; \
	fi
	@if pgrep -f pulseaudio >/dev/null; then \
		echo "✓ PulseAudio process is running"; \
	else \
		echo "✗ PulseAudio process is not running"; \
	fi
	@if [ -f ./conf/pulse-client.conf ]; then \
		echo "✓ PulseAudio client config exists"; \
	else \
		echo "✗ PulseAudio client config missing"; \
	fi
	@if [ -f ./conf/pulse-cookie ]; then \
		echo "✓ PulseAudio cookie exists in conf directory"; \
	else \
		echo "✗ PulseAudio cookie missing from conf directory"; \
	fi
endif 