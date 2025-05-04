in vec2 TexCoord;
uniform sampler2D audio_history_texture;
uniform int num_channels;

int get_audio_history_size() {
    ivec2 audio_size = textureSize(audio_history_texture, 0);
    return audio_size.x * audio_size.y / num_channels;
}

float get_audio_history_sample(int index, int channel) {
    ivec2 audio_size = textureSize(audio_history_texture, 0);
    ivec2 index_real = ivec2(index % audio_size.x, index / audio_size.x * num_channels + channel);

    return texture(audio_history_texture, vec2(index_real) / vec2(audio_size)).r;
}