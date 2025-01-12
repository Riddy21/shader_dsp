
float generateNoise(float tone, float time) {
    // Generate random numbers between -1.0 and 1.0 based on current time
    return 2.0 * fract(sin(dot(vec2(time, global_time_val), vec2(12.9898, 78.233))) * 43758.5453) - 1.0;
}

void main() {
    float start_time = calculateTime(play_position, vec2(0.0, 0.0));
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0));
    float time = calculateTime(global_time_val, TexCoord);
    float noise_out = generateNoise(tone, time) * adsr_envelope(start_time, end_time, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(noise_out * gain, 0.0, 0.0, 0.0);
}