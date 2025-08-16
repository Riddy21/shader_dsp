#!/bin/bash

# Audio test script for Docker container
# This script tests audio playback through the selected device

echo "=== Audio Test ==="
echo ""

# Check PulseAudio connection
if ! pactl info > /dev/null 2>&1; then
    echo "ERROR: Cannot connect to PulseAudio server"
    exit 1
fi

# Show current default sink
current_sink=$(pactl info | grep "Default Sink" | cut -d: -f2 | xargs)
echo "Testing audio on default sink: $current_sink"
echo ""

# Test options
echo "Choose a test option:"
echo "1. Speaker test (stereo test tone)"
echo "2. Sine wave test (440Hz)"
echo "3. White noise test"
echo "4. Custom frequency test"
echo ""

read -p "Enter test option (1-4): " test_option

case $test_option in
    1)
        echo "Running speaker test (stereo test tone)..."
        echo "You should hear a test tone through both left and right channels."
        echo "Press Ctrl+C to stop the test."
        speaker-test -D pulse -t wav -c 2 -l 1
        ;;
    2)
        echo "Running sine wave test (440Hz)..."
        echo "You should hear a 440Hz tone."
        echo "Press Ctrl+C to stop the test."
        speaker-test -D pulse -t sine -f 440 -l 1
        ;;
    3)
        echo "Running white noise test..."
        echo "You should hear white noise."
        echo "Press Ctrl+C to stop the test."
        speaker-test -D pulse -t sine -f 1000 -l 1
        ;;
    4)
        read -p "Enter frequency in Hz (e.g., 1000): " frequency
        echo "Running custom frequency test (${frequency}Hz)..."
        echo "You should hear a ${frequency}Hz tone."
        echo "Press Ctrl+C to stop the test."
        speaker-test -D pulse -t sine -f "$frequency" -l 1
        ;;
    *)
        echo "Invalid option. Running default speaker test..."
        speaker-test -D pulse -t wav -c 2 -l 1
        ;;
esac

echo ""
echo "Audio test completed." 