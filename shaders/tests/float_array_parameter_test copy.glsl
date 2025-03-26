uniform float input_float_array[512];
layout(location = 1) out vec4 output_debug_texture;

void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord);
    // print all of the input array values in the output_debug_texture;
    output_debug_texture = vec4(input_float_array[int(TexCoord.x * 512.0)], 0.0, 0.0, 1.0);
}