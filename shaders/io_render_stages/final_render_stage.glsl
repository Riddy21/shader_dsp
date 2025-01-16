layout(location = 1) out vec4 final_output_texture;

void main() {
    final_output_texture = vec4(0.0, 0.0, 0.0, 0.0);
    output_audio_texture = texture(stream_audio_texture, TexCoord);
}