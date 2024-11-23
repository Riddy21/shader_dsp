float generateSquare(float tone, float time) {
    return sign(sin(2.0 * 3.14159265359 * tone * time));
}

void main() {
    float time = calculateTime(global_time_val, TexCoord, buffer_size);
    float square_out = generateSquare(tone, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(square_out * gain, 0.0, 0.0, 0.0);
}