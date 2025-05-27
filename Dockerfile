FROM node:20-alpine

ARG TZ=UTC
ENV TZ="${TZ}"

# Install basic dependencies including X11, SDL2 and GL dependencies
RUN apk update && apk add --no-cache \
    git \
    procps \
    sudo \
    zsh \
    man-pages \
    unzip \
    gnupg \
    curl \
    wget \
    bind-tools \
    jq \
    ripgrep \
    xvfb \
    xvfb-run \
    mesa-gl \
    mesa-dev \
    mesa-dri-gallium \
    sdl2-dev \
    sdl2_ttf-dev \
    sdl2_image-dev \
    sdl2_gfx-dev \
    sdl2_mixer-dev \
    glew-dev \
    freeglut-dev \
    g++ \
    scons \
    portaudio-dev \
    libx11-dev \
    iptables \
    iproute2 \
    fzf \
    github-cli \
    cmake \
    make

# Install Catch2 from source
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

# Create non-root user
RUN addgroup -g 2000 developer && \
    adduser -D -u 2000 -G developer -s /bin/zsh developer && \
    mkdir -p /etc/sudoers.d && \
    echo "developer ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/developer && \
    chmod 0440 /etc/sudoers.d/developer

# Ensure developer user has access to /usr/local/share
RUN mkdir -p /usr/local/share/npm-global && \
    chown -R developer:developer /usr/local/share

ARG USERNAME=developer

# Persist command history
RUN mkdir -p /commandhistory && \
    touch /commandhistory/.zsh_history && \
    mkdir -p /home/$USERNAME && \
    ln -s /commandhistory/.zsh_history /home/$USERNAME/.zsh_history && \
    chown -R $USERNAME:$USERNAME /commandhistory /home/$USERNAME/.zsh_history

# Set `DEVCONTAINER` environment variable to help with orientation
ENV DEVCONTAINER=true

# Create workspace and config directories and set permissions
RUN mkdir -p /workspace /home/$USERNAME/.claude && \
    chown -R $USERNAME:$USERNAME /workspace /home/$USERNAME/.claude

WORKDIR /workspace

# Set up non-root user
USER $USERNAME

# Install global packages
ENV NPM_CONFIG_PREFIX=/usr/local/share/npm-global
ENV PATH=$PATH:/usr/local/share/npm-global/bin

# Install Claude Code CLI
RUN npm install -g @anthropic-ai/claude-code

# Create a simple wrapper script to fix the shebang issue
RUN mkdir -p /usr/local/share/npm-global/bin \
    && echo '#!/bin/sh' > /usr/local/share/npm-global/bin/claude-wrapper \
    && echo 'exec /usr/local/bin/node --no-warnings --enable-source-maps /usr/local/share/npm-global/lib/node_modules/@anthropic-ai/claude-code/cli.js "$@"' >> /usr/local/share/npm-global/bin/claude-wrapper \
    && chmod +x /usr/local/share/npm-global/bin/claude-wrapper \
    && (mv /usr/local/share/npm-global/bin/claude /usr/local/share/npm-global/bin/claude-original 2>/dev/null || true) \
    && ln -sf /usr/local/share/npm-global/bin/claude-wrapper /usr/local/share/npm-global/bin/claude

# Set environment variables for Claude
ENV CLAUDE_CONFIG_DIR=/home/$USERNAME/.claude
ENV NODE_OPTIONS="--max-old-space-size=4096"

# Set the default shell to zsh
ENV SHELL /bin/zsh

# Setup zsh configuration
RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended \
    && echo 'export PATH=$PATH:/usr/local/share/npm-global/bin' >> ~/.zshrc \
    && echo 'export PATH=$PATH:/workspace/build/bin' >> ~/.zshrc \
    && echo '# Auto-build on login' >> ~/.zshrc \
    && echo 'if [ -f /workspace/SConstruct ] && [ ! -f /workspace/.build_completed ]; then' >> ~/.zshrc \
    && echo '  echo "Building project on first login..."' >> ~/.zshrc \
    && echo '  cd /workspace && scons -j15 && touch /workspace/.build_completed' >> ~/.zshrc \
    && echo 'fi' >> ~/.zshrc

CMD ["/bin/zsh"]