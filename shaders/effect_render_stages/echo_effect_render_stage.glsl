uniform int num_echos;
uniform float delay;
uniform float decay;
uniform sampler2D echo_audio_texture;

void main() {
    int delay_in_samples = int(delay * float(sample_rate));

    vec4 echo = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 1; i <= num_echos; i++) {
        float decay_factor = pow(decay, float(i));
        echo += get_tape_history_samples(TexCoord, speed_in_samples_per_buffer, tape_position - (delay_in_samples * i)) * decay_factor;
    }

    output_audio_texture = texture(stream_audio_texture, TexCoord) + echo;
    //debug_audio_texture = vec4(pow(decay, float(1)), 0.0, 0.0,0.0);
}