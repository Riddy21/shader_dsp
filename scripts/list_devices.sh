#!/bin/bash

# List audio devices script
# This script shows all available audio devices

echo "=== Audio Devices List ==="
echo ""

# Check PulseAudio connection
if ! pactl info > /dev/null 2>&1; then
    echo "ERROR: Cannot connect to PulseAudio server"
    exit 1
fi

# Show server info
echo "PulseAudio Server:"
echo "------------------"
pactl info | grep -E "(Server String|Protocol|Default Sink|Default Source)"
echo ""

# List sinks (output devices)
echo "Audio Sinks (Output Devices):"
echo "-----------------------------"
if pactl list short sinks | grep -q .; then
    pactl list short sinks | while read -r line; do
        sink_id=$(echo "$line" | awk '{print $1}')
        sink_name=$(echo "$line" | awk '{print $2}')
        sink_desc=$(echo "$line" | awk '{for(i=3;i<=NF;i++) printf "%s ", $i; print ""}')
        echo "  ID: $sink_id | Name: $sink_name"
        echo "      Description: $sink_desc"
        
        # Show if this is the default sink
        current_sink=$(pactl info | grep "Default Sink" | cut -d: -f2 | xargs)
        if [ "$sink_name" = "$current_sink" ]; then
            echo "      ✓ DEFAULT SINK"
        fi
        echo ""
    done
else
    echo "  No audio sinks found"
fi

# List sources (input devices)
echo "Audio Sources (Input Devices):"
echo "------------------------------"
if pactl list short sources | grep -q .; then
    pactl list short sources | while read -r line; do
        source_id=$(echo "$line" | awk '{print $1}')
        source_name=$(echo "$line" | awk '{print $2}')
        source_desc=$(echo "$line" | awk '{for(i=3;i<=NF;i++) printf "%s ", $i; print ""}')
        echo "  ID: $source_id | Name: $source_name"
        echo "      Description: $source_desc"
        
        # Show if this is the default source
        current_source=$(pactl info | grep "Default Source" | cut -d: -f2 | xargs)
        if [ "$source_name" = "$current_source" ]; then
            echo "      ✓ DEFAULT SOURCE"
        fi
        echo ""
    done
else
    echo "  No audio sources found"
fi

# Show detailed sink information
echo "Detailed Sink Information:"
echo "-------------------------"
pactl list sinks | grep -E "(Sink|Description|State|Volume|Mute)" | head -20 