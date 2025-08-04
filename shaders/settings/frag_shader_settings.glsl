in vec2 TexCoord;

// Invert the y coordinate
uniform sampler2D stream_audio_texture;
uniform int buffer_size;
uniform int sample_rate;
uniform int num_channels;

layout(std140) uniform global_time {
    highp int global_time_val;
};

layout(location = 0) out vec4 output_audio_texture;
layout(location = 1) out vec4 debug_audio_texture;