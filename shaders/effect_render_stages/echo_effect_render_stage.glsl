uniform int num_echos;
uniform float delay;
uniform float decay;
uniform sampler2D echo_audio_texture;

void main() {
    ivec2 echo_buffer_size = ivec2(textureSize(echo_audio_texture, 0));
    float echo_sample_data_size_in_seconds = (float(echo_buffer_size.y/2) - 1.0) * float(buffer_size) / float(sample_rate);
    float delay_increment = delay / echo_sample_data_size_in_seconds;

    vec4 echo = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < num_echos; i++) {
        float echo_sample_index = float(i+1) * delay_increment;
        float decay_factor = pow(decay, float(i));
        if (echo_sample_index >= 1.0 || decay_factor < 0.001) {
            break;
        }
        vec2 echo_sample_coord = vec2(TexCoord.x, echo_sample_index);
        //echo += texture(echo_audio_texture, echo_sample_coord) * pow(decay, float(i));
        echo += texture(echo_audio_texture, echo_sample_coord);
    }

    output_audio_texture = texture(stream_audio_texture, TexCoord) + echo;
}