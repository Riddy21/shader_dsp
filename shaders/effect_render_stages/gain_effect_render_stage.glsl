uniform float gains[8]; // Array of gains, one per channel (max 8 channels)

void main() {
    // Calculate which channel we're in based on Y coordinate
    // Each channel occupies 1/num_channels of the texture height
    float channel_height = 1.0 / float(num_channels);
    int channel_index = int(TexCoord.y / channel_height);
    
    // Clamp channel index to valid range
    channel_index = clamp(channel_index, 0, num_channels - 1);
    
    // Get the gain for this specific channel
    float channel_gain = gains[channel_index];

    // Output the result with per-channel gain
    output_audio_texture = texture(stream_audio_texture, TexCoord) * channel_gain;
    debug_audio_texture = output_audio_texture;
}