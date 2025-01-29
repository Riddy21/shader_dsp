void main() {
    // Sample the audio texture
    float audioSample = texture(stream_audio_texture, TexCoord).r;

    // Map the audio sample to the y-coordinate
    float y = (audioSample + 1.0) / 2.0; // Assuming audioSample is in range [-1, 1]

    // Flip the y-coordinate
    y = 1.0 - y;

    // Draw a line at the y-coordinate
    if (abs(TexCoord.y - y) < 0.003) {
        output_audio_texture = vec4(1.0, 1.0, 1.0, 1.0); // White line
    } else {
        output_audio_texture = vec4(0.0, 0.0, 0.0, 1.0); // Black background
    }
}