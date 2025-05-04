in vec2 TexCoord;
uniform int num_taps;
uniform sampler2D b_coeff_texture;
uniform sampler2D stream_audio_texture;
uniform int buffer_size;
uniform int num_channels;
out vec4 output_audio_texture;
float get_audio_history_sample(int, int);
int get_audio_history_size();

float get_tex_value(sampler2D tex, int index) {
    ivec2 index_real = ivec2(index % buffer_size, index / buffer_size);
    ivec2 texture_size = textureSize(tex, 0);
    return texture(tex, vec2(index_real) / vec2(texture_size)).r;
}

// function for joining the history and the current audio stream into one index to pull from
float get_data(int index) {
    int channel = int(TexCoord.y * float(num_channels));
    // Get the last num_taps - 1 samples of history and concat with current stream_audio_texture
    int total_history_size = get_audio_history_size();
    int history_size = num_taps - 1;
    if (index < history_size) {
        // History texture
        int history_index = index + total_history_size - history_size;
        return get_audio_history_sample(history_index, channel);
    } else {
        // stream texture
        int history_index = index - history_size;
        return texture(stream_audio_texture, vec2(float(history_index)/float(buffer_size), TexCoord.y)).r;
    }
}

void main() {
    // Map the horizontal coordinate to an output index.
    int outputIndex = int(TexCoord.x * float(buffer_size));

    float y = 0.0;
    // Loop over the FIR taps. Note: many GLSL compilers require the loop bound to be constant.
    for (int i = 0; i < num_taps; i++) {
        // Retrieve the FIR coefficient from the b_coeff_texture.
        float b_val = get_tex_value(b_coeff_texture, i);
        // Get the corresponding sample from the extended input:
        // extended_input[outputIndex - i] (this automatically reverses the window relative to b)
        float smpl = get_data(outputIndex - i);
        y += b_val * smpl;
    }
    
    // Output the result to the red channel.

    output_audio_texture = vec4(y, 0.0, 0.0, 0.0);
}
