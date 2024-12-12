uniform sampler2D stream_audio_texture_1;
uniform sampler2D stream_audio_texture_2;
uniform sampler2D stream_audio_texture_3;

// TODO: Add new parameter type that combines multiple texture stages

void main() {
    vec4 audio_texture_1 = texture(stream_audio_texture_1, TexCoord);
    vec4 audio_texture_2 = texture(stream_audio_texture_2, TexCoord);
    vec4 audio_texture_3 = texture(stream_audio_texture_3, TexCoord);
    vec4 audio_texture = texture(stream_audio_texture, TexCoord);
    output_audio_texture = audio_texture_1 + audio_texture_2 + audio_texture_3 + audio_texture;
}