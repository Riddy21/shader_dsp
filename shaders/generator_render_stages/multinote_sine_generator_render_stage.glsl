float generateSine(float tone) {
    float phase = calculatePhase(global_time_val, TexCoord, tone);

    return sin(TWO_PI * tone * phase);
}

void main() {
    output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < active_notes; i++) {
        float start_time = calculateTimeSimple(play_positions[i]);
        float end_time = calculateTimeSimple(stop_positions[i]);
        float time = calculateTime(global_time_val, TexCoord);
        float sine_out = generateSine(tones[i]) * adsr_envelope(start_time, end_time, time);

        output_audio_texture += vec4(sine_out * gains[i], 0.0, 0.0, 0.0);
    }
    output_audio_texture += texture(stream_audio_texture, TexCoord);
    debug_audio_texture = output_audio_texture;
}

