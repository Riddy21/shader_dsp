#!/bin/bash

# Performance optimization script for OpenGL applications in Docker
# This script should be run inside the container to optimize performance

set -e

echo "🔧 Optimizing performance for OpenGL applications..."

# Function to check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "❌ This script must be run as root for full optimization"
        echo "   Some optimizations will be skipped"
        return 1
    fi
    return 0
}

# Function to set process priority
set_process_priority() {
    local pid=$1
    local priority=$2
    
    if [[ -n "$pid" && -n "$priority" ]]; then
        echo "📈 Setting priority $priority for process $pid"
        renice -n $priority -p $pid 2>/dev/null || echo "⚠️  Could not set priority for process $pid"
    fi
}

# Function to set CPU affinity
set_cpu_affinity() {
    local pid=$1
    local cpu_mask=$2
    
    if [[ -n "$pid" && -n "$cpu_mask" ]]; then
        echo "🔗 Setting CPU affinity $cpu_mask for process $pid"
        taskset -p $cpu_mask $pid 2>/dev/null || echo "⚠️  Could not set CPU affinity for process $pid"
    fi
}

# Function to optimize system settings
optimize_system() {
    echo "⚙️  Optimizing system settings..."
    
    # Set I/O scheduler to deadline for better real-time performance
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
    
    # Set real-time priority limits
    echo -1 > /proc/sys/kernel/sched_rt_runtime_us 2>/dev/null || true
    echo -1 > /proc/sys/kernel/sched_rt_period_us 2>/dev/null || true
}

# Function to optimize X11 settings
optimize_x11() {
    echo "🖥️  Optimizing X11 settings..."
    
    # Set X11 performance hints
    export SDL_VIDEO_X11_NODIRECTCOLOR=1
    export SDL_VIDEO_X11_XRANDR=1
    export SDL_VIDEO_X11_XVIDMODE=1
    export SDL_VIDEO_X11_XINERAMA=1
    export SDL_VIDEO_X11_XDBE=1
    export SDL_VIDEO_X11_DGAMOUSE=0
    
    # Disable X11 extensions that might cause stuttering
    export SDL_VIDEO_X11_XSHAPE=0
    export SDL_VIDEO_X11_XCURSOR=0
}

# Function to optimize OpenGL settings
optimize_opengl() {
    echo "🎮 Optimizing OpenGL settings..."
    
    # Enable hardware acceleration when possible
    export LIBGL_ALWAYS_SOFTWARE=0
    export MESA_GLTHREAD=1
    export MESA_DEBUG=0
    
    # Set optimal Mesa settings
    export MESA_GL_VERSION_OVERRIDE=3.3
    export MESA_GLSL_VERSION_OVERRIDE=330
    
    # Disable vsync for maximum performance (can be re-enabled if needed)
    export SDL_VIDEO_X11_NODIRECTCOLOR=1
}

# Function to create performance wrapper
create_performance_wrapper() {
    echo "📝 Creating performance wrapper script..."
    
    cat > /usr/local/bin/run_with_priority << 'EOF'
#!/bin/bash

# Performance wrapper script for OpenGL applications
# Usage: run_with_priority <command> [args...]

if [[ $# -eq 0 ]]; then
    echo "Usage: run_with_priority <command> [args...]"
    exit 1
fi

# Set high priority for the process
exec nice -n -10 ionice -c 1 -n 0 "$@"
EOF

    chmod +x /usr/local/bin/run_with_priority
    echo "✅ Performance wrapper created at /usr/local/bin/run_with_priority"
}

# Function to optimize current process
optimize_current_process() {
    echo "🎯 Optimizing current process..."
    
    # Set high priority for current shell
    renice -n -10 -p $$ 2>/dev/null || echo "⚠️  Could not set priority for current process"
    
    # Set CPU affinity to use all available cores
    taskset -p -c 0-3 $$ 2>/dev/null || echo "⚠️  Could not set CPU affinity for current process"
}

# Function to display system information
show_system_info() {
    echo "📊 System Information:"
    echo "   CPU cores: $(nproc)"
    echo "   Memory: $(free -h | grep Mem | awk '{print $2}')"
    echo "   Current priority: $(nice)"
    echo "   CPU governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'unknown')"
    echo "   I/O scheduler: $(cat /sys/block/*/queue/scheduler 2>/dev/null | head -1 || echo 'unknown')"
}

# Main execution
main() {
    echo "🚀 Starting performance optimization..."
    
    # Check if running as root
    is_root=$(check_root)
    
    # Optimize system settings (requires root)
    if [[ $is_root -eq 0 ]]; then
        optimize_system
    fi
    
    # Optimize environment variables
    optimize_x11
    optimize_opengl
    
    # Optimize current process
    optimize_current_process
    
    # Create performance wrapper
    create_performance_wrapper
    
    # Show system information
    show_system_info
    
    echo ""
    echo "✅ Performance optimization complete!"
    echo ""
    echo "📋 Usage tips:"
    echo "   - Run your OpenGL app with: run_with_priority <your_app>"
    echo "   - For maximum performance, run this script before starting your app"
    echo "   - Monitor performance with: htop, iotop, or glxgears"
    echo ""
    echo "🔧 Additional optimizations:"
    echo "   - Consider using 'taskset -c 0-3 <your_app>' to bind to specific CPUs"
    echo "   - Use 'chrt -f -p 99 <pid>' to set real-time priority (requires root)"
    echo "   - Monitor GPU usage with: glxinfo | grep 'OpenGL renderer'"
}

# Run main function
main "$@" 