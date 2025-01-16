in vec2 TexCoord;

uniform sampler2D stream_audio_texture;
uniform int buffer_size;
uniform int sample_rate;

layout(std140) uniform global_time {
    int global_time_val;
};

layout(location = 0) out vec4 output_audio_texture;