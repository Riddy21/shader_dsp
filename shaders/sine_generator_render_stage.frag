float generateSine(float tone, float time) {
    return sin(TWO_PI * tone * time);
}

void main() {
    float start_time = calculateTime(play_position, vec2(0.0, 0.0), buffer_size);
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0), buffer_size);
    float time = calculateTime(global_time_val, TexCoord, buffer_size);
    float sine_out = generateSine(tone, time) * adsr_envelope(start_time, end_time, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(sine_out * gain, 0.0, 0.0, 0.0);
}

