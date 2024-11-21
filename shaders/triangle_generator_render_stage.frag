float generateTriangle(float tone, float time) {
    return 2.0 * abs(2.0 * tone * time - 2.0 * floor(tone * time + 0.5)) - 1.0;
}

void main() {
    int elapsed_time = global_time_val - play_position;

    float time = calculateTime(elapsed_time, TexCoord, buffer_size);
    float triangle_out = generateTriangle(tone, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(triangle_out * gain, 0.0, 0.0, 0.0);
}