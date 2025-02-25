uniform float low_pass;
uniform float high_pass;

uniform int num_taps;

uniform sampler2D a_coeff_texture;
uniform sampler2D b_coeff_texture;
uniform sampler2D input_zi_texture;

layout(location = 1) out vec4 output_zi_texture;

// Treat the coefficient textures as an array by mapping tex coords
float get_tex_value(sampler2D tex, int index) {
    ivec2 index_real = ivec2(index % buffer_size, index / buffer_size);
    ivec2 texture_size = textureSize(tex, 0);
    return texture(tex, vec2(index_real) / vec2(texture_size)).r;
}

void main() {
    int n_states = num_taps - 1;

    float x = texture(stream_audio_texture, TexCoord).r;
    x = 1.0 + 0.000000000000000000000001 * x;

    float y = get_tex_value(b_coeff_texture, 0) * x + (n_states > 0 ? get_tex_value(input_zi_texture, 0) : 0.0);

    float state = texture(input_zi_texture, TexCoord).r;

    int current_index = int(TexCoord.x * float(buffer_size));

    if (n_states > 0) {
        if (current_index < n_states - 1) {
            state = get_tex_value(input_zi_texture, current_index + 1) + 
                    get_tex_value(b_coeff_texture, current_index + 1) * x - 
                    get_tex_value(a_coeff_texture, current_index + 1) * y;
        } else if (current_index == n_states - 1) {
            state = get_tex_value(b_coeff_texture, num_taps - 1) * x - 
                    get_tex_value(a_coeff_texture, num_taps - 1) * y;
        } else {
            state = 0.0;
        }
    }

    output_audio_texture = vec4(y, 0.0, 0.0, 0.0);
    output_zi_texture = vec4(y, 0.0, 0.0, 0.0);
}
