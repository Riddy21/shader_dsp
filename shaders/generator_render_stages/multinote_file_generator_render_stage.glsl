in vec2 TexCoord;
uniform int active_notes;
uniform int play_positions[24];
uniform int stop_positions[24];
uniform float tones[24];
uniform float gains[24];
uniform int global_time_val;
uniform sampler2D stream_audio_texture;
uniform sampler2D full_audio_data_texture;
uniform int buffer_size, num_channels;
out vec4 output_audio_texture;
float adsr_envelope(float, float, float);
float calculateTime(int, vec2);
const float MIDDLE_C = 261.63;

vec2 translate_coord(vec2 coord, int time, float speed, int channel) {
    // Get the chunk size
    ivec2 audio_size = textureSize(full_audio_data_texture, 0);

    int total_audio_size = audio_size.x * audio_size.y / 2 / num_channels; // divide by 2 because spacing

    ivec2 chunk_size = ivec2(float(buffer_size) * speed, 1);

    // Calculate the offset
    int chunk_offset = time * chunk_size.x % total_audio_size; // repeat
    //int chunk_offset = time * chunk_size.x; // no repeat

    int total_offset = int(coord.x * float(chunk_size.x)) + chunk_offset;

    ivec2 total_coord = ivec2(total_offset % audio_size.x,
                              total_offset / audio_size.x);

    // Comput normalized texture coordinates
    // FIXME: Add the channel offset to the y coordinate as well
    vec2 data = vec2(float(total_coord.x) / float(audio_size.x),
                4.0 * (float(total_coord.y) + 0.5 * float(channel) + 0.25) / float(audio_size.y));
                // For some reason, only the above line works for raspberry pi
                 //4.0 * (float(total_coord.y) + 0.25 * coord.y) / float(audio_size.y));

    return data;
}

void main() {
    output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < active_notes; i++) {
        float start_time = calculateTime(play_positions[i], vec2(0.0, 0.0));
        float end_time = calculateTime(stop_positions[i], vec2(0.0, 0.0));
        float time = calculateTime(global_time_val, TexCoord);

        int channel = int(TexCoord.y * float(num_channels));
        
        // Translate the texture coordinates
        vec2 coord = translate_coord(TexCoord, global_time_val - play_positions[i], tones[i] / MIDDLE_C, channel);

        // Get the audio sample
        vec4 audio_sample = texture(full_audio_data_texture, coord) * adsr_envelope(start_time, end_time, time);

        // Output the result
        output_audio_texture += audio_sample * gains[i];
    }
    output_audio_texture += texture(stream_audio_texture, vec2(TexCoord));
}
