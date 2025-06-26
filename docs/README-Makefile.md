# Shader DSP Development Environment - Makefile Guide

This Makefile consolidates all the functionality from the original `run-container.sh` script and individual audio scripts into a comprehensive build system.

## Quick Start

### Complete Setup (Recommended)
```bash
make setup    # Complete initial setup (build + container)
make connect  # Connect to container (PulseAudio setup happens automatically)
make all      # Complete setup + audio configuration
```

### Clean Shutdown
```bash
make clean    # Stop container and PulseAudio server
```

## Available Commands

### Setup and Management
- `make setup` - Complete setup (build + start container)
- `make all` - Complete setup and audio configuration
- `make build` - Build/rebuild the container
- `make up` - Start the container in detached mode
- `make down` - Stop and remove the container
- `make connect` - Connect to the running container shell (auto PulseAudio setup)
- `make rebuild` - Rebuild and restart the container
- `make clean` - Stop container and PulseAudio, clean up

### Audio Management
- `make pulse-setup` - Setup PulseAudio server on host (macOS only, with status tracking)
- `make pulse-stop` - Stop PulseAudio server and remove status file
- `make audio` - Setup and test audio (PulseAudio)
- `make audio-test` - Run audio test script in container
- `make audio-list` - List audio devices in container

### Testing
- `make test` - Test the AudioSynthesizer application
- `make status` - Show current status of container and PulseAudio

### Help
- `make help` - Show this help information

## Platform-Specific Behavior

### macOS
- Automatically sets up XQuartz for X11 forwarding
- **PulseAudio setup happens automatically when connecting** (`make connect`)
- Uses status file (`.pulse_audio_ready`) to prevent re-running setup
- Uses `host.docker.internal:0` for display
- Requires XQuartz installation: `brew install --cask xquartz`

### Linux
- Uses native X11 display (`:0`)
- Automatically detects ALSA devices
- No PulseAudio setup required (uses system audio)

### Windows
- Uses Windows video driver
- No X11 setup required
- No PulseAudio setup required

## Prerequisites

### macOS
1. Install XQuartz:
   ```bash
   brew install --cask xquartz
   ```
2. Enable "Allow connections from network clients" in XQuartz preferences
3. Restart your system

### All Platforms
1. Docker Desktop installed and running
2. Docker Compose available

## Workflow Examples

### First Time Setup
```bash
# Complete setup (PulseAudio will be set up when you connect)
make setup

# Connect to container (PulseAudio setup happens automatically)
make connect
```

### Daily Development
```bash
# Start container
make up

# Connect to container (PulseAudio setup happens automatically if needed)
make connect

# Test audio
make audio-test

# Clean shutdown
make clean
```

### Audio Troubleshooting
```bash
# Check status
make status

# Restart PulseAudio (macOS)
make pulse-stop
make connect  # This will re-run pulse-setup

# Test audio
make audio-test
```

## Environment Variables

The Makefile automatically sets these environment variables based on your platform:

- `DISPLAY` - X11 display (macOS: `host.docker.internal:0`, Linux: `:0`)
- `SDL_VIDEODRIVER` - SDL video driver (`x11` for macOS/Linux)
- `ALSA_DEVICES` - ALSA device path (Linux: `/dev/snd` if available, otherwise `/dev/null`)

## Container Management

### Starting the Container
```bash
make up
```
This will:
1. Check if Docker is running
2. Setup X11 for macOS
3. Start the container in detached mode

### Connecting to Container
```bash
make connect
```
This will:
1. Check if Docker is running
2. **Automatically setup PulseAudio on macOS (if not already done)**
3. Check if container is running (start if needed)
4. Connect to the container shell
5. Provide fallback commands if direct connection fails

### Stopping the Container
```bash
make down
```
This will stop and remove the container.

## Audio Setup

### Automatic PulseAudio Setup (macOS)
PulseAudio setup now happens automatically when you run `make connect`:

1. **Status File Tracking**: Uses `.pulse_audio_ready` file to track setup status
2. **Automatic Detection**: Checks if PulseAudio is already set up
3. **No Re-run**: Won't re-run setup if already completed
4. **Manual Override**: Use `make pulse-stop` to force re-setup

```bash
# First time - PulseAudio will be set up automatically
make connect

# Subsequent times - Setup will be skipped (status file exists)
make connect

# Force re-setup
make pulse-stop
make connect
```

### Container Audio Setup
```bash
make audio
```
This will:
1. Ensure container is running
2. Ensure PulseAudio is set up (macOS)
3. Run the audio setup script inside the container
4. Allow device selection and configuration

## Status File System

The Makefile uses a status file (`.pulse_audio_ready`) to track PulseAudio setup:

- **Created**: When PulseAudio setup completes successfully
- **Contains**: Timestamp of when setup was completed
- **Checked**: Before running PulseAudio setup
- **Removed**: When running `make pulse-stop` or `make clean`

### Benefits:
- **Prevents Re-running**: Setup only happens once
- **Faster Connections**: Subsequent `make connect` commands are faster
- **Clear Status**: `make status` shows setup status
- **Manual Control**: Can force re-setup with `make pulse-stop`

## Troubleshooting

### Container Won't Start
```bash
# Check Docker status
docker info

# Check container logs
docker-compose logs shader-dsp

# Rebuild container
make rebuild
```

### Audio Issues
```bash
# Check PulseAudio status (macOS)
make status

# Force re-setup PulseAudio (macOS)
make pulse-stop
make connect  # This will re-run pulse-setup

# Test audio in container
make audio-test
```

### X11 Issues (macOS)
```bash
# Check XQuartz installation
ls /Applications/Utilities/XQuartz.app

# Install XQuartz if missing
brew install --cask xquartz

# Restart and try again
make setup
```

### Status File Issues
```bash
# Check if status file exists
ls -la .pulse_audio_ready

# Remove status file to force re-setup
rm .pulse_audio_ready
make connect

# Or use the proper command
make pulse-stop
make connect
```

## Migration from run-container.sh

The Makefile replaces all functionality from `run-container.sh`:

| Old Command | New Command |
|-------------|-------------|
| `./run-container.sh build` | `make build` |
| `./run-container.sh up` | `make up` |
| `./run-container.sh down` | `make down` |
| `./run-container.sh connect` | `make connect` |
| `./run-container.sh rebuild` | `make rebuild` |
| `./run-container.sh test` | `make test` |
| `./run-container.sh audio` | `make audio` |
| `./run-container.sh audio-test` | `make audio-test` |
| `./run-container.sh audio-list` | `make audio-list` |
| `./run-container.sh pulse-setup` | `make connect` (automatic) |

## Benefits of the Makefile System

1. **Consolidated Management** - All commands in one place
2. **Automatic Prerequisites** - PulseAudio setup happens automatically
3. **Status Tracking** - Prevents unnecessary re-setup
4. **Platform Detection** - Automatic OS-specific configuration
5. **Clean Shutdown** - Proper cleanup of all services
6. **Status Monitoring** - Easy status checking
7. **Error Handling** - Better error messages and recovery
8. **Dependency Management** - Proper target dependencies
9. **Cross-Platform** - Works on macOS, Linux, and Windows 