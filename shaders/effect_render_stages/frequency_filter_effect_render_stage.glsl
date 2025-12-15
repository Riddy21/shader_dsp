uniform int num_taps;

uniform sampler2D b_coeff_texture;

// Treat the coefficient textures as an array by mapping tex coords
float get_tex_value(sampler2D tex, int index) {
    ivec2 index_real = ivec2(index % buffer_size, index / buffer_size);
    ivec2 texture_size = textureSize(tex, 0);
    return texture(tex, (vec2(index_real) + 0.5) / vec2(texture_size)).r;
}

// function for joining the history and the current audio stream into one index to pull from
float get_data(int index) {
    int channel = int(TexCoord.y * float(num_channels));
    // Get the last num_taps - 1 samples of history and concat with current stream_audio_texture
    int history_size = num_taps - 1;
    
    if (index < history_size) {
        // We are in the history section
        // index 0 corresponds to the oldest history sample (back(history_size))
        // index history_size-1 corresponds to the newest history sample (back(1))
        return get_tape_history_sample_from_back(history_size - index, channel);
    } else {
        // stream texture
        int stream_index = index - history_size;
        return texture(stream_audio_texture, vec2((float(stream_index) + 0.5)/float(buffer_size), TexCoord.y)).r;
    }
}

void main() {
    // Map the horizontal coordinate to an output index.
    int outputIndex = int(TexCoord.x * float(buffer_size));

    float y = 0.0;
    int history_size = num_taps - 1;

    // Loop over the FIR taps. Note: many GLSL compilers require the loop bound to be constant.
    for (int i = 0; i < num_taps; i++) {
        // Retrieve the FIR coefficient from the b_coeff_texture.
        float b_val = get_tex_value(b_coeff_texture, i);
        // Get the corresponding sample from the extended input:
        // We want sample at current_index - tap_index (convolution)
        // In our get_data coordinate system:
        //   index < history_size is history
        //   index >= history_size is current buffer (starting at offset history_size)
        // So current buffer sample 'outputIndex' is at 'outputIndex + history_size'
        // And we want to shift back by 'i'
        float smpl = get_data(outputIndex + history_size - i);
        y += b_val * smpl;
    }
    
    // Output the result to the red channel.
    int channel = int(TexCoord.y * float(num_channels));

    output_audio_texture = vec4(y, 0.0, 0.0, 0.0);
    debug_audio_texture = texture(stream_audio_texture, TexCoord);
}
