#!/bin/bash

# Host-side Docker performance optimization script
# This script should be run on the host machine to optimize Docker container performance

set -e

CONTAINER_NAME="shader-dsp-shader-dsp-1"  # Default Docker Compose container name
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "🐳 Optimizing Docker container performance..."

# Function to check if Docker is running
check_docker() {
    if ! docker info >/dev/null 2>&1; then
        echo "❌ Docker is not running or not accessible"
        exit 1
    fi
}

# Function to find the correct container name
find_container() {
    local container_name=$1
    
    # Try to find the container
    if docker ps --format "table {{.Names}}" | grep -q "$container_name"; then
        echo "$container_name"
        return 0
    fi
    
    # Try alternative naming patterns
    local alternatives=(
        "shader-dsp_1"
        "shader-dsp-shader-dsp-1"
        "workspace-shader-dsp-1"
        "shader-dsp"
    )
    
    for alt in "${alternatives[@]}"; do
        if docker ps --format "table {{.Names}}" | grep -q "$alt"; then
            echo "$alt"
            return 0
        fi
    done
    
    echo "❌ Could not find container. Available containers:"
    docker ps --format "table {{.Names}}"
    return 1
}

# Function to optimize Docker daemon settings
optimize_docker_daemon() {
    echo "⚙️  Optimizing Docker daemon settings..."
    
    # Check if we can modify Docker daemon settings
    if [[ ! -w /etc/docker/daemon.json ]]; then
        echo "⚠️  Cannot modify Docker daemon settings (requires root)"
        echo "   Consider running: sudo $0"
        return 1
    fi
    
    # Create optimized daemon.json
    cat > /etc/docker/daemon.json << 'EOF'
{
    "default-ulimits": {
        "nofile": {
            "name": "nofile",
            "hard": 65536,
            "soft": 65536
        }
    },
    "storage-driver": "overlay2",
    "log-driver": "json-file",
    "log-opts": {
        "max-size": "10m",
        "max-file": "3"
    },
    "default-shm-size": "1G",
    "default-address-pools": [
        {
            "base": "172.17.0.0/12",
            "size": 24
        }
    ]
}
EOF

    echo "✅ Docker daemon settings updated"
    echo "   Restart Docker daemon to apply changes: sudo systemctl restart docker"
}

# Function to set container resource limits
set_container_limits() {
    local container_name=$1
    
    echo "📊 Setting container resource limits..."
    
    # Get container ID
    local container_id=$(docker inspect --format='{{.Id}}' "$container_name" 2>/dev/null)
    if [[ -z "$container_id" ]]; then
        echo "❌ Could not get container ID for $container_name"
        return 1
    fi
    
    # Set CPU limits (requires root)
    if [[ $EUID -eq 0 ]]; then
        echo "🔧 Setting CPU limits..."
        
        # Set CPU quota and period for better performance
        echo 400000 > "/sys/fs/cgroup/cpu/docker/$container_id/cpu.cfs_quota_us" 2>/dev/null || true
        echo 100000 > "/sys/fs/cgroup/cpu/docker/$container_id/cpu.cfs_period_us" 2>/dev/null || true
        
        # Set CPU affinity to specific cores (adjust based on your system)
        echo "0-3" > "/sys/fs/cgroup/cpuset/docker/$container_id/cpuset.cpus" 2>/dev/null || true
        
        # Set memory limits
        echo "8G" > "/sys/fs/cgroup/memory/docker/$container_id/memory.limit_in_bytes" 2>/dev/null || true
        
        echo "✅ Container resource limits set"
    else
        echo "⚠️  Cannot set container limits (requires root)"
        echo "   Consider running: sudo $0"
    fi
}

# Function to optimize host system for Docker
optimize_host_system() {
    echo "🖥️  Optimizing host system for Docker..."
    
    # Set I/O scheduler to deadline for better performance
    if [[ -d /sys/block ]]; then
        for device in /sys/block/*/queue/scheduler; do
            if [[ -f "$device" ]]; then
                echo "deadline" > "$device" 2>/dev/null || true
            fi
        done
    fi
    
    # Optimize memory management
    echo 1 > /proc/sys/vm/drop_caches 2>/dev/null || true
    echo 0 > /proc/sys/vm/swappiness 2>/dev/null || true
    
    # Optimize CPU governor for performance
    if [[ -d /sys/devices/system/cpu ]]; then
        for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            if [[ -f "$cpu" ]]; then
                echo "performance" > "$cpu" 2>/dev/null || true
            fi
        done
    fi
    
    # Increase file descriptor limits
    echo "* soft nofile 65536" >> /etc/security/limits.conf 2>/dev/null || true
    echo "* hard nofile 65536" >> /etc/security/limits.conf 2>/dev/null || true
    
    echo "✅ Host system optimized"
}

# Function to restart container with optimized settings
restart_container_optimized() {
    local container_name=$1
    
    echo "🔄 Restarting container with optimized settings..."
    
    # Stop the container
    docker stop "$container_name" 2>/dev/null || true
    
    # Start with optimized settings
    docker start "$container_name"
    
    echo "✅ Container restarted"
}

# Function to show container performance info
show_container_info() {
    local container_name=$1
    
    echo "📊 Container Performance Information:"
    echo "   Container: $container_name"
    echo "   Status: $(docker inspect --format='{{.State.Status}}' "$container_name" 2>/dev/null || echo 'unknown')"
    echo "   CPU usage: $(docker stats --no-stream --format 'table {{.CPUPerc}}' "$container_name" 2>/dev/null || echo 'unknown')"
    echo "   Memory usage: $(docker stats --no-stream --format 'table {{.MemUsage}}' "$container_name" 2>/dev/null || echo 'unknown')"
    echo "   Network I/O: $(docker stats --no-stream --format 'table {{.NetIO}}' "$container_name" 2>/dev/null || echo 'unknown')"
}

# Function to create optimized docker-compose override
create_optimized_compose() {
    echo "📝 Creating optimized docker-compose override..."
    
    cat > docker-compose.override.yml << 'EOF'
version: '3.8'

services:
  shader-dsp:
    # Performance optimizations
    ulimits:
      nofile:
        soft: 65536
        hard: 65536
      nproc:
        soft: 65536
        hard: 65536
    
    # Resource limits
    deploy:
      resources:
        limits:
          cpus: '4.0'
          memory: 8G
        reservations:
          cpus: '2.0'
          memory: 2G
    
    # Performance settings
    group_add:
      - video
    
    security_opt:
      - seccomp:unconfined
    
    # Shared memory size
    shm_size: 1G
    
    # Environment optimizations
    environment:
      - SDL_VIDEO_X11_NODIRECTCOLOR=1
      - SDL_VIDEO_X11_XRANDR=1
      - SDL_VIDEO_X11_XVIDMODE=1
      - SDL_VIDEO_X11_XINERAMA=1
      - SDL_VIDEO_X11_XDBE=1
      - SDL_VIDEO_X11_DGAMOUSE=0
      - LIBGL_ALWAYS_SOFTWARE=0
      - MESA_GLTHREAD=1
      - MESA_DEBUG=0
      - OMP_NUM_THREADS=4
      - MKL_NUM_THREADS=4
      - OPENBLAS_NUM_THREADS=4
EOF

    echo "✅ Optimized docker-compose override created"
    echo "   Run 'docker-compose up -d' to apply changes"
}

# Main execution
main() {
    echo "🚀 Starting Docker performance optimization..."
    
    # Check Docker
    check_docker
    
    # Find container
    local container_name=$(find_container "$CONTAINER_NAME")
    if [[ $? -ne 0 ]]; then
        exit 1
    fi
    
    echo "📦 Found container: $container_name"
    
    # Optimize host system (requires root)
    if [[ $EUID -eq 0 ]]; then
        optimize_host_system
        optimize_docker_daemon
        set_container_limits "$container_name"
    else
        echo "⚠️  Some optimizations require root privileges"
        echo "   Consider running: sudo $0"
    fi
    
    # Create optimized compose override
    create_optimized_compose
    
    # Show container info
    show_container_info "$container_name"
    
    echo ""
    echo "✅ Docker performance optimization complete!"
    echo ""
    echo "📋 Next steps:"
    echo "   1. Restart Docker daemon: sudo systemctl restart docker"
    echo "   2. Rebuild and restart container: docker-compose up -d --build"
    echo "   3. Run performance optimization inside container:"
    echo "      docker exec -it $container_name performance_optimize.sh"
    echo "   4. Run your OpenGL app with priority:"
    echo "      docker exec -it $container_name run_with_priority <your_app>"
    echo ""
    echo "🔧 Monitoring commands:"
    echo "   - Container stats: docker stats $container_name"
    echo "   - Host performance: htop, iotop"
    echo "   - GPU info: docker exec -it $container_name glxinfo | grep 'OpenGL renderer'"
}

# Run main function
main "$@" 