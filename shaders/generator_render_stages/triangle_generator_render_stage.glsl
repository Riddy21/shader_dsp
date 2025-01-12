float generateTriangle(float tone) {
    float phase = calculatePhase(global_time_val, TexCoord, tone);
    return 2.0 * abs(2.0 * tone * phase - 2.0 * floor(tone * phase + 0.5)) - 1.0;
}

void main() {
    float start_time = calculateTime(play_position, vec2(0.0, 0.0));
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0));
    float time = calculateTime(global_time_val, TexCoord);
    float triangle_out = generateTriangle(tone) * adsr_envelope(start_time, end_time, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(triangle_out * gain, 0.0, 0.0, 0.0);
}