FROM debian:bookworm-slim

ARG TZ=UTC

# Update package lists and install basic dependencies
RUN apt-get update && apt-get install -y \
    git \
    procps \
    sudo \
    bash \
    unzip \
    gnupg \
    curl \
    wget \
    dnsutils \
    jq \
    ripgrep \
    xvfb \
    mesa-utils \
    libgl1-mesa-dev \
    libgl1-mesa-glx \
    libglu1-mesa-dev \
    libsdl2-dev \
    libsdl2-ttf-dev \
    libsdl2-image-dev \
    libsdl2-gfx-dev \
    libsdl2-mixer-dev \
    libglew-dev \
    freeglut3-dev \
    g++ \
    scons \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libxfixes-dev \
    libxrender-dev \
    libxss-dev \
    libxxf86vm-dev \
    libxkbcommon-dev \
    iptables \
    iproute2 \
    fzf \
    cmake \
    make \
    build-essential \
    pkg-config \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libv4l-dev \
    libxvidcore-dev \
    libx264-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libatlas-base-dev \
    gfortran \
    python3 \
    python3-pip \
    python3-dev \
    # Add PulseAudio packages (exactly as in SDL example)
    pulseaudio \
    pulseaudio-utils \
    alsa-utils \
    libasound2-plugins \
    && rm -rf /var/lib/apt/lists/*

# Install Node.js (simulating Pi environment)
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs

# Install Catch2 from source (compatible with Pi environment)
RUN cd /tmp && \
    git clone https://github.com/catchorg/Catch2.git && \
    cd Catch2 && \
    git checkout v3.5.4 && \
    mkdir -p build && \
    cd build && \
    cmake .. -DBUILD_TESTING=OFF -DCATCH_BUILD_STATIC_LIBRARY=ON && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/Catch2

# Install Pi-specific libraries and tools (simulated for x86_64)
RUN apt-get update && apt-get install -y \
    python3-gpiozero \
    # Add performance monitoring tools
    htop \
    iotop \
    sysstat \
    cpufrequtils \
    && rm -rf /var/lib/apt/lists/*

# Create symbolic links to simulate Pi libraries
RUN mkdir -p /opt/vc/lib /opt/vc/include /opt/vc/bin \
    && ln -sf /usr/lib/x86_64-linux-gnu/libbcm_host.so /opt/vc/lib/libbcm_host.so 2>/dev/null || true \
    && ln -sf /usr/lib/x86_64-linux-gnu/libvcos.so /opt/vc/lib/libvcos.so 2>/dev/null || true \
    && ln -sf /usr/lib/x86_64-linux-gnu/libvchiq_arm.so /opt/vc/lib/libvchiq_arm.so 2>/dev/null || true \
    && ln -sf /usr/include/bcm_host.h /opt/vc/include/bcm_host.h 2>/dev/null || true

# Create XDG runtime directories
RUN mkdir -p /tmp/runtime-root /tmp/data /tmp/config /tmp/cache && \
    chmod 700 /tmp/runtime-root

# Create pulse audio user and group (handle existing group) - exactly as in SDL example
RUN groupadd -r pulse 2>/dev/null || true && \
    useradd -r -g pulse pulse 2>/dev/null || true

# Create necessary directories - exactly as in SDL example
RUN mkdir -p /etc/pulse /root/.config/pulse /home/pulse/.config/pulse

# Copy pulse audio configuration - exactly as in SDL example
COPY conf/pulse-client.conf /etc/pulse/client.conf

# Copy audio setup script - exactly as in SDL example
COPY scripts/audio_setup.sh /usr/local/bin/audio_setup.sh
RUN chmod +x /usr/local/bin/audio_setup.sh

# Copy device listing script - exactly as in SDL example
COPY scripts/list_devices.sh /usr/local/bin/list_devices.sh
RUN chmod +x /usr/local/bin/list_devices.sh

# Copy audio test script - exactly as in SDL example
COPY scripts/test_audio.sh /usr/local/bin/test_audio.sh
RUN chmod +x /usr/local/bin/test_audio.sh

# Copy performance optimization script
COPY scripts/performance_optimize.sh /usr/local/bin/performance_optimize.sh
RUN chmod +x /usr/local/bin/performance_optimize.sh

# Copy Cursor sync/link script
COPY scripts/link_cursor_server.sh /usr/local/bin/link_cursor_server.sh
RUN chmod +x /usr/local/bin/link_cursor_server.sh

# Create workspace directory
RUN mkdir -p /workspace

WORKDIR /workspace

# Install Claude Code CLI
RUN npm install -g @anthropic-ai/claude-code

# Create a simple wrapper script to fix the shebang issue
RUN mkdir -p /usr/local/share/npm-global/bin \
    && echo '#!/bin/bash' > /usr/local/share/npm-global/bin/claude-wrapper \
    && echo 'exec /usr/bin/node --no-warnings --enable-source-maps /usr/local/share/npm-global/lib/node_modules/@anthropic-ai/claude-code/cli.js "$@"' >> /usr/local/share/npm-global/bin/claude-wrapper \
    && chmod +x /usr/local/share/npm-global/bin/claude-wrapper \
    && (mv /usr/local/share/npm-global/bin/claude /usr/local/share/npm-global/bin/claude-original 2>/dev/null || true) \
    && ln -sf /usr/local/share/npm-global/bin/claude-wrapper /usr/local/share/npm-global/bin/claude

# Set default command
CMD ["/bin/bash"]