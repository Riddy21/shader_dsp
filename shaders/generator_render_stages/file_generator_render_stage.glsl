int calculate_tape_position(int current_position, int start_position, int speed_in_samples_per_buffer) {
    int difference = current_position - start_position;
    return difference * speed_in_samples_per_buffer;
}

void main() {
    // Get the audio sample from stream
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);

    float start_time = calculateTime(play_position, vec2(0.0, 0.0));
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0));
    float time = calculateTime(global_time_val, TexCoord);

    // Calculate speed in samples per buffer from tone
    float speed_ratio = tone / MIDDLE_C;
    // Round to nearest integer, and ensure exact buffer_size when speed_ratio is 1.0 (MIDDLE_C)
    int speed_in_samples_per_buffer;
    if (abs(speed_ratio - 1.0) < 0.0001) {
        speed_in_samples_per_buffer = buffer_size;
    } else {
        speed_in_samples_per_buffer = int(float(buffer_size) * speed_ratio + 0.5); // Round to nearest
    }
    
    // Calculate tape position based on play position and speed
    int tape_position_samples = calculate_tape_position(global_time_val, play_position, speed_in_samples_per_buffer);

    // Get the tape sample using get_tape_history_samples
    vec4 tape_sample = vec4(0.0, 0.0, 0.0, 0.0);
    if (tape_position_samples >= 0) {
        tape_sample = get_tape_history_samples(TexCoord, speed_in_samples_per_buffer, tape_position_samples);
    }

    // Apply ADSR envelope
    vec4 audio_sample = tape_sample * adsr_envelope(start_time, end_time, time);

    // Output the result
    output_audio_texture = audio_sample * gain + stream_audio;
    debug_audio_texture = output_audio_texture;
}