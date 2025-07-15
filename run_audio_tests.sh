#!/bin/bash

# Audio Generator Render Stage Audio Output Tests
# This script runs the audio output tests that allow you to hear the generated audio

echo "Running Audio Generator Render Stage Direct Audio Output Tests..."
echo "These tests connect the audio generator directly to audio output (no renderer/graph)."
echo "They will play audio through your speakers/headphones."
echo "Make sure your audio is not muted and volume is at a comfortable level."
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the project root directory"
    exit 1
fi

# Build the project if needed
echo "Building project..."
if ! make -j$(nproc); then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "Running audio output tests..."
echo ""

# Run the specific audio output tests
# The [audio_output] tag filters to only run the audio output tests
./build/tests/audio_generator_render_stage_gl_test "[audio_output]"

echo ""
echo "Audio tests complete!"
echo ""
echo "What you should have heard:"
echo "1. A single A4 note (440 Hz) playing for 3 seconds"
echo "2. A C major scale (C D E F G A B C) with each note playing for 0.5 seconds"
echo "3. Three major chords (C major, F major, G major) each playing for 2 seconds"
echo ""
echo "These tests use direct connection from render stage to audio output (no renderer/graph)."
echo "If you heard clean, smooth sine waves without distortion or clipping, the tests passed!"
echo "If you heard any crackling, popping, or distortion, there may be issues with the audio generation." 