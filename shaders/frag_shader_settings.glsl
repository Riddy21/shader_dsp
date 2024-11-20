in vec2 TexCoord;

uniform sampler2D stream_audio_texture;

layout(std140) uniform global_time {
    int global_time_val;
};

out vec4 output_audio_texture;
