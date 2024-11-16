#version 300 es
precision highp float;

// Default parameters that must exist
in vec2 TexCoord;
uniform sampler2D stream_audio_texture;
layout(std140) uniform global_time {
    int global_time_val;
};
out vec4 output_audio_texture;

// Generator specific parameters
uniform int play_position;
uniform float tone;
uniform float gain;
uniform int buffer_size;

float calculateTime(int elapsed_time, vec2 TexCoord, int buffer_size) {
    // FIXME: Change 44100 to a default audio parameter
    return (float(elapsed_time) + TexCoord.x) / (44100.0 / float(buffer_size));
}

float generateSine(float tone, float time) {
    return sin(2.0 * 3.14159265359 * tone * time);
}

void main() {
    int elapsed_time = global_time_val - play_position;
    ivec2 texture_size = textureSize(stream_audio_texture, 0);

    float time = calculateTime(elapsed_time, TexCoord, buffer_size);
    float sine_out = generateSine(tone, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(sine_out * gain, 0.0, 0.0, 0.0);
}

