FROM debian:bookworm-slim

ARG TZ=UTC
ENV TZ="${TZ}"

# Simulate Raspberry Pi 5 environment variables
ENV RASPBERRY_PI_SIMULATION=true
ENV PI_ARCH=arm64
ENV PI_MODEL=5

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
    && rm -rf /var/lib/apt/lists/*

# Install Node.js (simulating Pi environment)
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs

# Install Catch2 from source (compatible with Pi environment)
RUN cd /tmp && \
    git clone https://github.com/catchorg/Catch2.git && \
    cd Catch2 && \
    git checkout v2.13.10 && \
    mkdir -p build && \
    cd build && \
    cmake .. -DBUILD_TESTING=OFF && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/Catch2

# Install Pi-specific libraries and tools (simulated for x86_64)
RUN apt-get update && apt-get install -y \
    python3-gpiozero \
    python3-rpi.gpio \
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

# Copy entrypoint script
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Set `DEVCONTAINER` environment variable to help with orientation
ENV DEVCONTAINER=true

# Create workspace directory
RUN mkdir -p /workspace

WORKDIR /workspace

# Install global packages
ENV NPM_CONFIG_PREFIX=/usr/local/share/npm-global
ENV PATH=$PATH:/usr/local/share/npm-global/bin

# Install Claude Code CLI
RUN npm install -g @anthropic-ai/claude-code

# Create a simple wrapper script to fix the shebang issue
RUN mkdir -p /usr/local/share/npm-global/bin \
    && echo '#!/bin/bash' > /usr/local/share/npm-global/bin/claude-wrapper \
    && echo 'exec /usr/bin/node --no-warnings --enable-source-maps /usr/local/share/npm-global/lib/node_modules/@anthropic-ai/claude-code/cli.js "$@"' >> /usr/local/share/npm-global/bin/claude-wrapper \
    && chmod +x /usr/local/share/npm-global/bin/claude-wrapper \
    && (mv /usr/local/share/npm-global/bin/claude /usr/local/share/npm-global/bin/claude-original 2>/dev/null || true) \
    && ln -sf /usr/local/share/npm-global/bin/claude-wrapper /usr/local/share/npm-global/bin/claude

# Set environment variables for Claude
ENV CLAUDE_CONFIG_DIR=/root/.claude
ENV NODE_OPTIONS="--max-old-space-size=2048"

# Set the default shell to bash
ENV SHELL /bin/bash

# Setup bash configuration with Pi-specific environment
RUN echo '# Raspberry Pi 5 simulation environment' >> ~/.bashrc \
    && echo 'export RASPBERRY_PI_SIMULATION=true' >> ~/.bashrc \
    && echo 'export PI_ARCH=arm64' >> ~/.bashrc \
    && echo 'export PI_MODEL=5' >> ~/.bashrc \
    && echo 'export PATH=$PATH:/usr/local/share/npm-global/bin' >> ~/.bashrc \
    && echo 'export PATH=$PATH:/workspace/build/bin' >> ~/.bashrc \
    && echo 'export LD_LIBRARY_PATH=/opt/vc/lib:$LD_LIBRARY_PATH' >> ~/.bashrc \
    && echo 'export PKG_CONFIG_PATH=/opt/vc/lib/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc \
    && echo '# XDG environment variables' >> ~/.bashrc \
    && echo 'export XDG_RUNTIME_DIR=/tmp/runtime-root' >> ~/.bashrc \
    && echo 'export XDG_DATA_HOME=/tmp/data' >> ~/.bashrc \
    && echo 'export XDG_CONFIG_HOME=/tmp/config' >> ~/.bashrc \
    && echo 'export XDG_CACHE_HOME=/tmp/cache' >> ~/.bashrc \
    && echo 'export SDL_VIDEODRIVER=x11' >> ~/.bashrc \
    && echo '# Auto-build on login (with Pi-optimized settings)' >> ~/.bashrc \
    && echo 'if [ -f /workspace/SConstruct ] && [ ! -f /workspace/.build_completed ]; then' >> ~/.bashrc \
    && echo '  echo "Building project on first login (Pi 5 simulation)..."' >> ~/.bashrc \
    && echo '  cd /workspace && scons -j4 && touch /workspace/.build_completed' >> ~/.bashrc \
    && echo 'fi' >> ~/.bashrc

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["/bin/bash"]