uniform sampler2D audio_history_texture;
uniform int tape_position;
uniform int speed_in_samples_per_buffer;
uniform int tape_window_size_samples;
uniform int tape_window_offset_samples;
uniform int tape_stopped; // 1 = stopped, 0 = playing

// Get the size of the audio history texture in samples per channel
int get_tape_history_size() {
    ivec2 audio_size = textureSize(audio_history_texture, 0);
    return audio_size.x * audio_size.y / num_channels;
}

// Get the audio section corresponding to the tape section in the tape history from the tape position (in samples)
// Returns the audio sample (vec4) from the audio_history_texture at the current tape position
// TexCoord: texture coordinates from the current shader (channel is encoded in TexCoord.y)
vec4 get_tape_history_samples(vec2 TexCoord) {
    // If tape is stopped, return zeros
    if (tape_stopped != 0) {
        return vec4(0.0);
    }
    
    // Get texture dimensions to calculate position bar
    ivec2 audio_size = textureSize(audio_history_texture, 0);

    // Get channel as int
    int channel = int(TexCoord.y * float(num_channels));

    // Calculate the position of the current position in the audio output texture
    int window_offset = int(TexCoord.x * float(speed_in_samples_per_buffer));
    int position_in_window = tape_position - tape_window_offset_samples + window_offset - 1;

    // Calculate the position of the current position in the audio output texture
    int audio_width = audio_size.x;
    int audio_height = audio_size.y / num_channels / 2; // 2 because we need to store both the audio data and the zeros

    // Clamp position_in_window to valid range [0, window_size_samples)
    // Handle negative values properly
    if (position_in_window < 0) {
        // Before window start - return zero
        return vec4(0.0);
    }
    if (position_in_window >= tape_window_size_samples) {
        // After window end - return zero
        return vec4(0.0);
    }

    // Calculate the x and y position of the current position in the audio output texture
    int x_position = position_in_window % audio_width;
    int y_row_position = position_in_window / audio_width;

    // Calculate y_position for each channel and check if we're on the correct one
    // Only highlight channel 0 to avoid duplicate lines
    int y_position = ( y_row_position * num_channels + channel) * 2;

    // Convert the x y into texture coordinates
    vec2 texture_coord = vec2(float(x_position) / float(audio_size.x), (float(y_position) + 0.5) / float(audio_size.y)); // Offset for max data
    
    return texture(audio_history_texture, texture_coord);
}

// Get the current tape position in samples
int get_tape_position_samples() {
    return tape_position;
}

// Get the current tape position in seconds
float get_tape_position_seconds() {
    return float(tape_position) / float(sample_rate);
}

// Get the tape speed multiplier (1.0 = normal speed, 2.0 = double speed, etc.)
// Computed from samples per buffer: speed = samples_per_buffer / buffer_size
float get_tape_speed() {
    return float(speed_in_samples_per_buffer) / float(buffer_size);
}

// Get the window size in samples (maximum amount of audio that can be played at once)
int get_tape_window_size_samples() {
    return tape_window_size_samples;
}

// Get the window size in seconds
float get_tape_window_size_seconds() {
    return float(tape_window_size_samples) / float(sample_rate);
}

