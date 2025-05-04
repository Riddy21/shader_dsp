in vec2 TexCoord;
uniform float input_float_array[512];
uniform sampler2D stream_audio_texture;
out vec4 output_audio_texture;
out vec4 output_debug_texture;

void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord);
    output_debug_texture = vec4(input_float_array[int(TexCoord.x * 512.0)], 0.0, 0.0, 1.0);
}