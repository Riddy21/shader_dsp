in vec2 TexCoord;
uniform int active_notes;
uniform int play_positions[24];
uniform int stop_positions[24];
uniform float tones[24];
uniform float gains[24];
uniform int global_time_val;
uniform sampler2D stream_audio_texture;
out vec4 output_audio_texture;
float adsr_envelope(float, float, float);
float calculateTime(int, vec2);
float calculateTimeSimple(int);
float calculatePhase(int, vec2, float);

float generateTriangle(float tone, float time) {
    float phase = calculatePhase(time, TexCoord, tone);
    return 2.0 * abs(2.0 * tone * phase - 2.0 * floor(tone * phase + 0.5)) - 1.0;
}

void main() {
    output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < active_notes; i++) {
        float start_time = calculateTimeSimple(play_positions[i]);
        float end_time = calculateTimeSimple(stop_positions[i]);
        float time = calculateTime(global_time_val, TexCoord);
        float triangle_out = generateTriangle(tones[i], time) * adsr_envelope(start_time, end_time, time);

        output_audio_texture += vec4(triangle_out * gains[i], 0.0, 0.0, 0.0);
    }
    output_audio_texture += texture(stream_audio_texture, TexCoord);
}
