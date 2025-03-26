float generateNoise(float tone, float time) {
    // Generate random numbers between -1.0 and 1.0 based on current time
    return 2.0 * fract(sin(dot(vec2(time, global_time_val), vec2(12.9898, 78.233))) * 43758.5453) - 1.0;
}

void main() {
    output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < active_notes; i++) {
        float start_time = calculateTimeSimple(play_positions[i]);
        float end_time = calculateTimeSimple(stop_positions[i]);
        float time = calculateTime(global_time_val, TexCoord);
        float noise_out = generateNoise(tones[i], time) * adsr_envelope(start_time, end_time, time);

        output_audio_texture += vec4(noise_out * gains[i], 0.0, 0.0, 0.0);
    }
    output_audio_texture += texture(stream_audio_texture, TexCoord);
}
