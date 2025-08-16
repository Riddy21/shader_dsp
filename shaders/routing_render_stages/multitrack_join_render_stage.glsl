uniform sampler2D stream_audio_texture_0;
uniform sampler2D stream_audio_texture_1;
uniform sampler2D stream_audio_texture_2;
uniform sampler2D stream_audio_texture_3;
uniform sampler2D stream_audio_texture_4;
uniform sampler2D stream_audio_texture_5;
uniform sampler2D stream_audio_texture_6;

// NOTE: Can't use array of textures as a uniform because it's not supported in GLSL 3.00 ES

void main() {
    vec4 audio_texture = texture(stream_audio_texture, TexCoord);
    vec4 audio_texture_0 = texture(stream_audio_texture_0, TexCoord);
    vec4 audio_texture_1 = texture(stream_audio_texture_1, TexCoord);
    vec4 audio_texture_2 = texture(stream_audio_texture_2, TexCoord);
    vec4 audio_texture_3 = texture(stream_audio_texture_3, TexCoord);
    vec4 audio_texture_4 = texture(stream_audio_texture_4, TexCoord);
    vec4 audio_texture_5 = texture(stream_audio_texture_5, TexCoord);
    vec4 audio_texture_6 = texture(stream_audio_texture_6, TexCoord);

    output_audio_texture = audio_texture +
                           audio_texture_0 + audio_texture_1 + audio_texture_2 +
                           audio_texture_3 + audio_texture_4 + audio_texture_5 +
                           audio_texture_6;
    debug_audio_texture = output_audio_texture;
}