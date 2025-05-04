in vec2 TexCoord;
uniform float tone, gain;
uniform int play_position, stop_position, global_time_val;
uniform sampler2D stream_audio_texture;
out vec4 output_audio_texture;
float adsr_envelope(float, float, float);
float calculateTime(int, vec2);
float calculatePhase(int, vec2, float);

float generateSawtooth(float tone) {
    float phase = calculatePhase(global_time_val, TexCoord, tone);

    return 2.0 * (tone * phase - floor(tone * phase + 0.5));
}

void main() {
    float start_time = calculateTime(play_position, vec2(0.0, 0.0));
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0));
    float time = calculateTime(global_time_val, TexCoord);
    float sawtooth_out = generateSawtooth(tone) * adsr_envelope(start_time, end_time, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(sawtooth_out * gain, 0.0, 0.0, 0.0);
}