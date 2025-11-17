uniform sampler2D tape_history_texture{PLUGIN_SUFFIX};
uniform int tape_position{PLUGIN_SUFFIX};
uniform int speed_in_samples_per_buffer{PLUGIN_SUFFIX};
uniform int tape_window_size_samples{PLUGIN_SUFFIX};
uniform int tape_window_offset_samples{PLUGIN_SUFFIX};
uniform int tape_stopped{PLUGIN_SUFFIX}; // 1 = stopped, 0 = playing

// Get the size of the audio history texture in samples per channel
int get_tape_history_size{PLUGIN_SUFFIX}() {
    ivec2 audio_size = textureSize(tape_history_texture{PLUGIN_SUFFIX}, 0);
    return audio_size.x * audio_size.y / num_channels;
}

// Get the audio section corresponding to the tape section in the tape history from the tape position (in samples)
// Returns the audio sample (vec4) from the tape_history_texture at the specified tape position
// TexCoord: texture coordinates from the current shader (channel is encoded in TexCoord.y)
// sample_frame_size: number of samples per frame (typically speed_in_samples_per_buffer)
// tape_position_param: the tape position in samples to read from
vec4 get_tape_history_samples{PLUGIN_SUFFIX}(vec2 TexCoord, int sample_frame_size, int tape_position_param) {
    // If tape is stopped, return zeros
    if (tape_stopped{PLUGIN_SUFFIX} != 0) {
        return vec4(0.0);
    }
    
    // Get texture dimensions to calculate position bar
    ivec2 audio_size = textureSize(tape_history_texture{PLUGIN_SUFFIX}, 0);

    // Get channel as int
    int channel = int(TexCoord.y * float(num_channels));

    // Calculate the position of the current position in the audio output texture
    int window_offset = int(TexCoord.x * float(sample_frame_size));
    // Special case: when tape_position = 0 and tape_window_offset_samples = 0, we need to add 1
    // to compensate for the -1 in the formula, so that window_offset = 0 maps to position_in_window = 0
    int position_in_window = tape_position_param - tape_window_offset_samples{PLUGIN_SUFFIX} + window_offset;
    if (sample_frame_size < 0) {
        position_in_window -= 1;
    }

    // Calculate the position of the current position in the audio output texture
    int audio_width = audio_size.x;
    int audio_height = audio_size.y / num_channels / 2; // 2 because we need to store both the audio data and the zeros

    // Return zeros for out-of-range positions
    // This ensures that positions before the window start or after the window end return zeros
    // This matches the behavior of the tape's playback_for_render_stage_history method
    if (position_in_window < 0) {
        // Before window start - position is out of range, return zero
        return vec4(0.0);
    }
    if (position_in_window >= tape_window_size_samples{PLUGIN_SUFFIX}) {
        // After window end - position is out of range, return zero
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
    
    return texture(tape_history_texture{PLUGIN_SUFFIX}, texture_coord);
}

// Overload for backward compatibility - uses global uniforms
vec4 get_tape_history_samples{PLUGIN_SUFFIX}(vec2 TexCoord) {
    return get_tape_history_samples{PLUGIN_SUFFIX}(TexCoord, speed_in_samples_per_buffer{PLUGIN_SUFFIX}, tape_position{PLUGIN_SUFFIX});
}

// Get the current tape position in samples
int get_tape_position_samples{PLUGIN_SUFFIX}() {
    return tape_position{PLUGIN_SUFFIX};
}

// Get the current tape position in seconds
float get_tape_position_seconds{PLUGIN_SUFFIX}() {
    return float(tape_position{PLUGIN_SUFFIX}) / float(sample_rate);
}

// Get the tape speed multiplier (1.0 = normal speed, 2.0 = double speed, etc.)
// Computed from samples per buffer: speed = samples_per_buffer / buffer_size
float get_tape_speed{PLUGIN_SUFFIX}() {
    return float(speed_in_samples_per_buffer{PLUGIN_SUFFIX}) / float(buffer_size);
}

// Get the window size in samples (maximum amount of audio that can be played at once)
int get_tape_window_size_samples{PLUGIN_SUFFIX}() {
    return tape_window_size_samples{PLUGIN_SUFFIX};
}

// Get the window size in seconds
float get_tape_window_size_seconds{PLUGIN_SUFFIX}() {
    return float(tape_window_size_samples{PLUGIN_SUFFIX}) / float(sample_rate);
}

