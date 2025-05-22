uniform int num_echos;
uniform float delay;
uniform float decay;
uniform sampler2D echo_audio_texture;

void main() {
    int channel = int(TexCoord.y * float(num_channels));
    ivec2 echo_buffer_size = ivec2(textureSize(echo_audio_texture, 0));
    float epsilon = 0.00001; // To prevent texel alignment issues

    int delay_in_samples = int(delay * float(sample_rate));
    int delay_in_rows = delay_in_samples / buffer_size;
    float delay_increment = float(delay_in_rows * num_channels) / float(echo_buffer_size.y);

    vec4 echo = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < num_echos; i++) {
        float channel_increment = float(channel) / float(echo_buffer_size.y);
        float echo_sample_index = float(i+1) * delay_increment * float(num_channels) + channel_increment + epsilon;
        float decay_factor = pow(decay, float(i));
        if (echo_sample_index >= 1.0 || decay_factor < 0.001) {
            break;
        }
        vec2 echo_sample_coord = vec2(TexCoord.x, echo_sample_index);
        echo += texture(echo_audio_texture, echo_sample_coord) * pow(decay, float(i));
    }

    output_audio_texture = texture(stream_audio_texture, TexCoord) + echo;
    debug_audio_texture = output_audio_texture;
}