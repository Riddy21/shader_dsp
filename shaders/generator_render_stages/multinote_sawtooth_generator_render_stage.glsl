float generateSawtooth(float tone) {
    float phase = calculatePhase(global_time_val, TexCoord, tone);

    return 2.0 * (tone * phase - floor(tone * phase + 0.5));
}

void main() {
    output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < active_notes; i++) {
        float start_time = calculateTimeSimple(play_positions[i]);
        float end_time = calculateTimeSimple(stop_positions[i]);
        float time = calculateTime(global_time_val, TexCoord);
        float sawtooth_out = generateSawtooth(tones[i]) * adsr_envelope(start_time, end_time, time);

        output_audio_texture += vec4(sawtooth_out * gains[i], 0.0, 0.0, 0.0);
    }
    output_audio_texture += texture(stream_audio_texture, TexCoord);
}