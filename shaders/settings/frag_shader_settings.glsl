in vec2 TexCoord;

// Invert the y coordinate
uniform sampler2D stream_audio_texture;
uniform int buffer_size;
uniform int sample_rate;
uniform int num_channels;
uniform int global_time_val;

out vec4 output_audio_texture;