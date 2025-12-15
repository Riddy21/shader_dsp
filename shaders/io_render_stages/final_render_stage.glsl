layout(location = 2) out vec4 final_output_audio_texture;

void main(){
    // Convert from interpolated coordinates to non-interpolated coordinates.
    int x_int = int(TexCoord.x * float(buffer_size));
    int y_int = int(TexCoord.y * float(num_channels));

    int position = y_int * buffer_size + x_int;

    // Calculate the sample and channel from the position.
    int channel = position % num_channels;
    int smpl = position / num_channels;

    vec2 inTexCoord = vec2(float(smpl) / float(buffer_size), float(channel) / float(num_channels));
    
    // Finally, sample the input texture at the rotated coordinate.
    output_audio_texture = texture(stream_audio_texture, inTexCoord);
    final_output_audio_texture = texture(stream_audio_texture, inTexCoord);
    debug_audio_texture = texture(stream_audio_texture, inTexCoord);
}
