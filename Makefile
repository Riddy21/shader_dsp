# Shader DSP Development Environment Makefile
# Simplified container management and development tasks

.PHONY: help build up down connect clean status check check-docker check-x11 check-pulse check-container

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
	@echo "  check   - Check if all services are running (without starting them)"
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
	@echo "  make check   - Check service status"
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
PULSE_SETUP_FILE := $(BUILD_DIR)/pulse-setup.done
CONTAINER_BUILT_FILE := $(BUILD_DIR)/container-built.done

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
		echo "Docker is installed but not running."; \
		echo "Please start Docker Desktop manually and run 'make build' again."; \
		exit 1; \
	else \
		echo "✓ Docker is already running and ready!"; \
	fi
	@echo "✓ Docker setup completed successfully!"
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

# Check if Docker is running
check-docker:
	@if docker info >/dev/null 2>&1; then \
		echo "✓ Docker is running"; \
		exit 0; \
	else \
		echo "✗ Docker is not running"; \
		exit 1; \
	fi

# Check if XQuartz is running (macOS only)
check-x11:
ifeq ($(PLATFORM),macos)
	@if pgrep -x "Xquartz" >/dev/null; then \
		echo "✓ XQuartz is running"; \
		exit 0; \
	else \
		echo "✗ XQuartz is not running"; \
		exit 1; \
	fi
else
	@echo "✓ X11 check not needed on $(PLATFORM)"; \
	exit 0;
endif

# Check if PulseAudio is running (macOS only)
check-pulse:
ifeq ($(PLATFORM),macos)
	@if pgrep -f pulseaudio >/dev/null; then \
		echo "✓ PulseAudio is running"; \
		exit 0; \
	else \
		echo "✗ PulseAudio is not running"; \
		exit 1; \
	fi
else
	@echo "✓ PulseAudio check not needed on $(PLATFORM)"; \
	exit 0;
endif

# Check if container is running
check-container:
	@if docker-compose ps | grep -q "Up"; then \
		echo "✓ Container is running"; \
		exit 0; \
	else \
		echo "✗ Container is not running"; \
		exit 1; \
	fi

# Start Docker if not running
start-docker: $(DOCKER_SETUP_FILE)
	@set -e; \
	if ! docker info >/dev/null 2>&1; then \
		echo "Starting Docker..."; \
		if [ "$(PLATFORM)" = "macos" ]; then \
			echo "Opening Docker Desktop application..."; \
			if [ -d "/Applications/Docker.app" ]; then \
				open -a Docker; \
			elif [ -d "/opt/homebrew/Applications/Docker.app" ]; then \
				open -a /opt/homebrew/Applications/Docker.app; \
			elif [ -d "/usr/local/Applications/Docker.app" ]; then \
				open -a /usr/local/Applications/Docker.app; \
			else \
				echo "Docker.app not found in standard locations. Please start Docker Desktop manually."; \
				echo "You can find it in your Applications folder or download from https://www.docker.com/products/docker-desktop"; \
				exit 1; \
			fi; \
			echo "Waiting for Docker Desktop to start and initialize..."; \
			echo "This may take a minute on first launch..."; \
			sleep 15; \
			for i in 1 2 3 4 5 6; do \
				if docker info >/dev/null 2>&1; then \
					echo "✓ Docker Desktop is now running and ready!"; \
					break; \
				fi; \
				echo "Still waiting for Docker Desktop to start... (attempt $$i/6)"; \
				if [ $$i -eq 3 ]; then \
					echo "Docker Desktop is taking longer than usual to start."; \
					echo "Please check if Docker Desktop is running in your Applications."; \
				fi; \
				sleep 10; \
			done; \
			if ! docker info >/dev/null 2>&1; then \
				echo ""; \
				echo "Error: Docker Desktop failed to start properly."; \
				echo "Please try the following:"; \
				echo "1. Open Docker Desktop manually from Applications"; \
				echo "2. Wait for it to fully start (you should see the Docker icon in your menu bar)"; \
				echo "3. Run 'make up' again"; \
				echo ""; \
				echo "If Docker Desktop is already running, try restarting it."; \
				exit 1; \
			fi; \
		else \
			echo "Error: Docker is not running. Please start Docker and try again."; \
			exit 1; \
		fi; \
	else \
		echo "✓ Docker is already running"; \
	fi

# Check all services (without starting them)
check: check-docker check-x11 check-pulse check-container
	@echo ""
	@echo "✓ All services are running!"

# Start XQuartz and Docker container
up: start-all

# Start X11 if not running
start-x11: $(X11_SETUP_FILE)
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
	else \
		echo "✓ XQuartz is already running"; \
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

# Start PulseAudio if not running
start-pulse: $(PULSE_SETUP_FILE)
ifeq ($(PLATFORM),macos)
	@set -e; \
	if ! pgrep -f pulseaudio >/dev/null; then \
		echo "Starting PulseAudio server on host (macOS)..."; \
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
		fi; \
	else \
		echo "✓ PulseAudio is already running"; \
	fi
else
	@echo "PulseAudio setup is only needed on macOS"
endif

# Start container if not running
start-container: $(CONTAINER_BUILT_FILE)
	@set -e; \
	if ! docker-compose ps | grep -q "Up"; then \
		echo "Starting Docker container..."; \
		docker-compose up -d; \
		echo "✓ Container started successfully!"; \
	else \
		echo "✓ Container is already running"; \
	fi

# Start everything (only starts what's not running)
start-all: start-x11 start-pulse start-docker start-container
	@echo ""
	@echo "✓ Everything started successfully!"
	@echo "Use 'make connect' to open a shell in the container."

# Connect to container shell
connect: up
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
	@set -e; \
	if docker-compose ps | grep -q "Up"; then \
		docker-compose down; \
		echo "✓ Container stopped"; \
	else \
		echo "✓ Container was not running"; \
	fi
	@echo ""
ifeq ($(PLATFORM),macos)
	@echo "Step 2: Stopping PulseAudio server..."
	@set -e; \
	if pgrep -f pulseaudio >/dev/null; then \
		pkill -f pulseaudio; \
		echo "✓ PulseAudio server stopped"; \
	else \
		echo "✓ PulseAudio was not running"; \
	fi
	@echo ""
	@echo "Step 3: Stopping XQuartz..."
	@set -e; \
	if pgrep -x "Xquartz" >/dev/null; then \
		pkill -x "Xquartz"; \
		echo "✓ XQuartz stopped"; \
	else \
		echo "✓ XQuartz was not running"; \
	fi
endif
	@echo ""
	@echo "✓ Everything stopped successfully!"

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
	@echo ""
	@echo "Service Status:"
	@if docker info >/dev/null 2>&1; then echo "✓ Docker is running"; else echo "✗ Docker is not running"; fi
ifeq ($(PLATFORM),macos)
	@if pgrep -x "Xquartz" >/dev/null; then echo "✓ XQuartz is running"; else echo "✗ XQuartz is not running"; fi
	@if pgrep -f pulseaudio >/dev/null; then echo "✓ PulseAudio is running"; else echo "✗ PulseAudio is not running"; fi
endif
	@if docker-compose ps | grep -q "Up"; then echo "✓ Container is running"; else echo "✗ Container is not running"; fi
	@echo ""
	@echo "Container Status:"
	@docker-compose ps 2>/dev/null || echo "Docker Compose not available"
	@echo ""
ifeq ($(PLATFORM),macos)
	@echo "PulseAudio Status:"
	@if command -v pulseaudio >/dev/null 2>&1; then \
		echo "✓ PulseAudio is installed"; \
	else \
		echo "✗ PulseAudio is not installed"; \
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