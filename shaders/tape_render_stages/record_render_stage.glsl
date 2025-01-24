uniform bool recording;
uniform int record_position;

void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord);
}