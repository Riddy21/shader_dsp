uniform sampler2D audio_history_texture;
uniform int tape_position;
uniform float tape_speed;
uniform float tape_window_size_seconds;

// Get the size of the audio history texture in samples per channel
int get_tape_history_size() {
    ivec2 audio_size = textureSize(audio_history_texture, 0);
    return audio_size.x * audio_size.y / num_channels;
}

// Get an audio sample from the tape history texture
// TexCoord: texture coordinate (TexCoord.x = sample index, TexCoord.y = channel)
// Uses tape_position and tape_speed internally to determine which sample to return
// TODO: Implement reading from tape history texture
// Need to discuss: How should the texture layout work? Should it match AudioRenderStageHistory format?
// Should we account for the tape position offset and speed when reading samples?
vec4 get_tape_history_sample(vec2 TexCoord) {
    // TODO: Implement texture sample reading
    // Extract sample index and channel from TexCoord
    // int sample_index = int(TexCoord.x * float(buffer_size));
    // int channel = int(TexCoord.y * float(num_channels));
    // 
    // Calculate the actual tape sample index based on:
    // - Current tape position (get_tape_position_samples())
    // - Current sample index in buffer
    // - Tape speed (get_tape_speed())
    // 
    // Then convert sample index and channel to texture coordinates
    // Consider: texture format, row layout, channel interleaving
    
    // Placeholder
    vec4 data = texture(audio_history_texture, TexCoord);

    return data;
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
float get_tape_speed() {
    return tape_speed;
}

// Get the window size in seconds (maximum amount of audio that can be played at once)
float get_tape_window_size_seconds() {
    return tape_window_size_seconds;
}

// Get the window size in samples
int get_tape_window_size_samples() {
    return int(tape_window_size_seconds * float(sample_rate));
}

