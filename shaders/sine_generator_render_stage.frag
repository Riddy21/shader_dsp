float generateSine(float tone, float time) {
    return sin(2.0 * 3.14159265359 * tone * time);
}

void main() {
    int elapsed_time = global_time_val - play_position;

    float time = calculateTime(elapsed_time, TexCoord, buffer_size);
    float sine_out = generateSine(tone, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(sine_out * gain, 0.0, 0.0, 0.0);
}

