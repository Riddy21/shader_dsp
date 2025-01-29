uniform sampler2D playback_texture;
uniform int play_position;
uniform bool play;
uniform int time_at_start;

void main() {
    vec4 playback = vec4(0.0, 0.0, 0.0, 0.0);

    if (play) {
        ivec2 playback_buffer_size = ivec2(textureSize(playback_texture, 0));

        int offset = (global_time_val - time_at_start + play_position) % (playback_buffer_size.y / 2); // divide by 2 because spacing

        float playback_sample_index =  float(offset * 2 + 1) / float(playback_buffer_size.y); // For raspberry pi
        //float playback_sample_index =  (float(offset * 2) + 0.5) / float(playback_buffer_size.y);

        //playback = texture(playback_texture, vec2(TexCoord.x, playback_sample_index));
        playback = texture(playback_texture, vec2(TexCoord.x, playback_sample_index));
    }

    output_audio_texture = texture(stream_audio_texture, TexCoord) + playback;
}