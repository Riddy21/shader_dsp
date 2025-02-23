uniform float low_pass;
uniform float high_pass;

const int order = 4;

// Apply a butterworth filter to the audio
void main() {
    output_audio_texture = texture(stream_audio_texture, TexCoord);
}