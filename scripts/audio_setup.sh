#!/bin/bash

# Audio setup script for Docker container
# This script sets up audio and allows device selection

echo "=== Docker Audio Setup ==="
echo ""

# Check if we can connect to PulseAudio server
echo "Checking PulseAudio server connection..."
if ! pactl info > /dev/null 2>&1; then
    echo "ERROR: Cannot connect to PulseAudio server at $PULSE_SERVER"
    echo "Make sure the host PulseAudio server is running and accessible"
    exit 1
fi

echo "✓ Connected to PulseAudio server"
echo ""

# Show server information
echo "PulseAudio Server Information:"
pactl info | grep -E "(Server String|Protocol|Default Sink|Default Source)"
echo ""

# List available audio devices
echo "=== Available Audio Devices ==="
echo ""

echo "Audio Sinks (Output Devices):"
echo "-----------------------------"
pactl list short sinks | while read -r line; do
    sink_id=$(echo "$line" | awk '{print $1}')
    sink_name=$(echo "$line" | awk '{print $2}')
    sink_desc=$(echo "$line" | awk '{for(i=3;i<=NF;i++) printf "%s ", $i; print ""}')
    echo "  ID: $sink_id | Name: $sink_name | Description: $sink_desc"
done

echo ""
echo "Audio Sources (Input Devices):"
echo "------------------------------"
pactl list short sources | while read -r line; do
    source_id=$(echo "$line" | awk '{print $1}')
    source_name=$(echo "$line" | awk '{print $2}')
    source_desc=$(echo "$line" | awk '{for(i=3;i<=NF;i++) printf "%s ", $i; print ""}')
    echo "  ID: $source_id | Name: $source_name | Description: $source_desc"
done

echo ""
echo "=== Device Selection ==="
echo ""

# Get current default sink
current_sink=$(pactl info | grep "Default Sink" | cut -d: -f2 | xargs)
echo "Current default sink: $current_sink"

# Ask user to select a sink
echo ""
echo "Available sink IDs:"
pactl list short sinks | awk '{print "  " $1 ": " $2}'

echo ""
read -p "Enter sink ID to set as default (or press Enter to keep current): " selected_sink

if [ -n "$selected_sink" ]; then
    echo "Setting default sink to ID: $selected_sink"
    pactl set-default-sink "$selected_sink"
    
    # Verify the change
    new_sink=$(pactl info | grep "Default Sink" | cut -d: -f2 | xargs)
    echo "New default sink: $new_sink"
else
    echo "Keeping current default sink: $current_sink"
fi

echo ""
echo "=== Audio Setup Complete ==="
echo "You can now use audio applications in this container."
echo "Run 'test_audio.sh' to test audio playback."
echo "Run 'list_devices.sh' to see devices again." 