float generateSquare(float tone, float time) {
    return sign(sin(2.0 * 3.14159265359 * tone * time));
}

void main() {
    int elapsed_time = global_time_val - play_position;
    ivec2 texture_size = textureSize(stream_audio_texture, 0);

    float time = calculateTime(elapsed_time, TexCoord, buffer_size);
    float square_out = generateSquare(tone, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(square_out * gain, 0.0, 0.0, 0.0);
}