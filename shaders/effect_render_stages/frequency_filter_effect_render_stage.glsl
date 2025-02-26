uniform float low_pass;
uniform float high_pass;

uniform int num_taps;

uniform sampler2D b_coeff_texture;
uniform sampler2D audio_history_texture;

// Treat the coefficient textures as an array by mapping tex coords
float get_tex_value(sampler2D tex, int index) {
    ivec2 index_real = ivec2(index % buffer_size, index / buffer_size);
    ivec2 texture_size = textureSize(tex, 0);
    return texture(tex, vec2(index_real) / vec2(texture_size)).r;
}

// function for joining the history and the current audio stream into one index to pull from
float get_data(int index) {
    // Get the last num_taps - 1 samples of history and concat with current stream_audio_texture
    int total_history_size = textureSize(audio_history_texture, 0).x;
    int history_size = num_taps - 1;
    if (index < history_size) {
        // History texture
        int history_index = index + total_history_size - history_size;
        return texture(audio_history_texture, vec2(float(history_index)/float(total_history_size), TexCoord.y)).r;
    } else {
        // stream texture
        int history_index = index - history_size;
        return texture(stream_audio_texture, vec2(float(history_index)/float(buffer_size), TexCoord.y)).r;
    }
}

void main() {
    int n_states = num_taps - 1;

    output_audio_texture = vec4(get_data(int(TexCoord.x * float(buffer_size))), 0.0, 0.0, 0.0) + texture(b_coeff_texture, TexCoord) * 0.0000001;
    //output_audio_texture = vec4(get_data(int(TexCoord.x * float(buffer_size))), 0.0, 0.0, 0.0) + texture(b_coeff_texture, TexCoord) * 0.0000001;
    //output_audio_texture = vec4(float(int(TexCoord.x * float(buffer_size))), 0.0, 0.0, 0.0) + texture(b_coeff_texture, TexCoord) * 0.0000001 * get_data(0);
}
