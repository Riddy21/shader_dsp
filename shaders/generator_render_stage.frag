#version 300 es
precision highp float;

in vec2 TexCoord;

uniform sampler2D full_audio_data_texture;

uniform sampler2D stream_audio_texture;

layout(std140) uniform global_time {
    int global_time_val;
};

uniform int play_position;

uniform float tone;

uniform float gain;

uniform int buffer_size;

out vec4 output_audio_texture;

vec2 translate_coord(vec2 coord) {
    // Get the chunk size
    ivec2 audio_size = textureSize(full_audio_data_texture, 0);

    int total_audio_size = audio_size.x * audio_size.y / 2; // divide by 2 because spacing

    ivec2 chunk_size = ivec2(float(buffer_size) * tone, 1);

    int chunk_offset = (global_time_val - play_position) * chunk_size.x % total_audio_size; // repeat
    //int chunk_offset = (global_time_val - play_position) * chunk_size.x; // no repeat

    int total_offset = int(coord.x * float(chunk_size.x)) + chunk_offset;

    ivec2 total_coord = ivec2(total_offset % audio_size.x,
                              total_offset / audio_size.x);

    // Comput normalized texture coordinates
    return vec2(float(total_coord.x) / float(audio_size.x),
                2.0 * (float(total_coord.y) + 0.25 * coord.y + 0.25) / float(audio_size.y));
                // For some reason, only the above line works for raspberry pi
                //2.0 * (float(total_coord.y) + 0.25 * coord.y) / float(audio_size.y));
}

void main() {
    // Translate the texture coordinates
    vec2 coord = translate_coord(TexCoord);

    // Get the audio sample
    vec4 audio_sample = texture(full_audio_data_texture, coord);

    // Output the result
    output_audio_texture = audio_sample * gain + texture(stream_audio_texture, TexCoord);
}