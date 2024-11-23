float generateSawtooth(float tone, float time) {
    return 2.0 * (tone * time - floor(tone * time + 0.5));
}

void main() {
    int elapsed_time = global_time_val - play_position;

    float time = calculateTime(elapsed_time, TexCoord, buffer_size);
    float sawtooth_out = generateSawtooth(tone, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(sawtooth_out * gain, 0.0, 0.0, 0.0);
}