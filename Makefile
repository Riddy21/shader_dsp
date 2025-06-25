# Shader DSP Development Environment Makefile
# Simplified container management and development tasks

.PHONY: help build up down connect clean status setup-pulse-cookie copy-pulse-cookie

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

# Complete setup - install dependencies and build container
build: install-docker setup-x11 install-pulse setup-pulse-cookie build-container
	@echo "✓ Complete setup finished!"
	@echo "Run 'make up' to start everything"

# Install Docker
install-docker:
	@echo "Installing Docker..."
	@echo "Checking Docker installation and status..."
	@if ! command -v docker >/dev/null 2>&1; then \
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

# Setup X11 for macOS
setup-x11:
	@echo "Setting up X11..."
ifeq ($(PLATFORM),macos)
	@echo "Setting up X11 for macOS..."
	@if ! command -v xquartz >/dev/null 2>&1 && ! ls /Applications/Utilities/XQuartz.app >/dev/null 2>&1; then \
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
	@if [ ! -d "/tmp/.X11-unix" ]; then \
		echo "Creating X11 socket directory..."; \
		sudo mkdir -p /tmp/.X11-unix; \
		sudo chmod 1777 /tmp/.X11-unix; \
	fi
	@echo "✓ X11 setup completed for macOS"
else
	@echo "X11 setup not needed on $(PLATFORM)"
endif

# Install PulseAudio
install-pulse:
	@echo "Installing PulseAudio..."
ifeq ($(PLATFORM),macos)
	@echo "Installing PulseAudio..."
	@if ! command -v pulseaudio >/dev/null 2>&1; then \
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

# Build the container
build-container:
	@echo "Building container..."
	@echo "Building container..."
	@echo "ALSA devices: $(ALSA_DEVICES)"
	@echo "SDL video driver: $(SDL_VIDEODRIVER)"
	@echo "Display: $(DISPLAY)"
	docker-compose build
	@echo "✓ Container built successfully!"

# Start XQuartz and Docker container
up: start-x11 setup-pulse
	@echo "Starting Docker container..."
	docker-compose up -d
	@echo ""
	@echo "✓ Everything started successfully!"
	@echo "Use 'make connect' to open a shell in the container."

# Start X11
start-x11:
	@echo "Starting XQuartz..."
ifeq ($(PLATFORM),macos)
	@echo "Starting XQuartz..."
	@if ! pgrep -x "Xquartz" >/dev/null; then \
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
			echo "Warning: XQuartz may not have started properly. Continuing anyway..."; \
		fi; \
	fi
	@if command -v xhost >/dev/null 2>&1; then \
		echo "Enabling X11 connections..."; \
		DISPLAY=:0 xhost +localhost; \
		DISPLAY=:0 xhost + 127.0.0.1; \
		echo "✓ X11 connection setup completed"; \
	fi
else
	@echo "X11 startup not needed on $(PLATFORM)"
endif

# Setup PulseAudio server on host (macOS)
setup-pulse:
	@echo "Setting up PulseAudio..."
ifeq ($(PLATFORM),macos)
	@echo "Setting up PulseAudio server on host (macOS)..."
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
	@echo "✓ PulseAudio server started successfully"
else
	@echo "PulseAudio setup is only needed on macOS"
endif

# Connect to container shell
connect:
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

# Stop container, PulseAudio, and XQuartz
down:
	@echo "=== Stopping Everything ==="
	@echo "Step 1: Stopping Docker container..."
	docker-compose down
	@echo "✓ Container stopped"
	@echo ""
ifeq ($(PLATFORM),macos)
	@echo "Step 2: Stopping PulseAudio server..."
	@pkill -f pulseaudio 2>/dev/null || true
	@echo "✓ PulseAudio server stopped"
	@echo ""
	@echo "Step 3: Stopping XQuartz..."
	@pkill -x "Xquartz" 2>/dev/null || true
	@echo "✓ XQuartz stopped"
endif
	@echo ""
	@echo "✓ Everything stopped successfully!"

# Complete cleanup
clean: down
	@echo "=== Complete Cleanup ==="
	@echo "Removing containers and images..."
	docker-compose down --rmi all --volumes --remove-orphans
	@echo "✓ Complete cleanup finished"

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