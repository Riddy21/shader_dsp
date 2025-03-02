float generateSquare(float tone) {
    float phase = calculatePhase(global_time_val, TexCoord, tone);

    return sign(sin(TWO_PI * tone * phase));
}

void main() {
    float start_time = calculateTime(play_position, vec2(0.0, 0.0));
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0));
    float time = calculateTime(global_time_val, TexCoord);
    float square_out = generateSquare(tone) * adsr_envelope(start_time, end_time, time);

    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(square_out * gain, 0.0, 0.0, 0.0);
}