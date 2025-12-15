void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord) + get_tape_history_samples(TexCoord);
}