#version 300 es
precision mediump float;
void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord);
    debug_audio_texture = texture(stream_audio_texture, TexCoord);
}