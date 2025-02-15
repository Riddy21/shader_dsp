uniform int num_echos;
uniform float delay;
uniform float decay;
uniform sampler2D echo_audio_texture;

void main() {
    int channel = int(TexCoord.y * float(num_channels));
    ivec2 echo_buffer_size = ivec2(textureSize(echo_audio_texture, 0));
    float epsilon = 0.0001; // To prevent texel alignment issues
    float echo_sample_data_size_in_seconds = (float(echo_buffer_size.y/num_channels) * float(buffer_size) + epsilon) / float(sample_rate);
    float delay_increment = delay / echo_sample_data_size_in_seconds;

    vec4 echo = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < num_echos; i++) {
        float channel_increment = float(channel) / float(echo_buffer_size.y);
        float echo_sample_index = float(i+1) * delay_increment + channel_increment;
        float decay_factor = pow(decay, float(i));
        if (echo_sample_index >= 1.0 || decay_factor < 0.001) {
            break;
        }
        vec2 echo_sample_coord = vec2(TexCoord.x, echo_sample_index);
        echo += texture(echo_audio_texture, echo_sample_coord) * pow(decay, float(i));
    }

    output_audio_texture = texture(stream_audio_texture, TexCoord) + echo;
}