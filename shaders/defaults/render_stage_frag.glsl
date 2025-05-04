in vec2 TexCoord;
uniform sampler2D stream_audio_texture;
out vec4 output_audio_texture;
void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord);
}