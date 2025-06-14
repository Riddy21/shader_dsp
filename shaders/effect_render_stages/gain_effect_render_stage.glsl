uniform float gain;
uniform float balance;

void main() {
    // Apply balance effect to the audio
    bool is_left = TexCoord.y < 0.5;
    // 0.0 is left, 1.0 is right
    float balance_effect = is_left ? 1.0 - balance : balance;

    //Output the result
    output_audio_texture = texture(stream_audio_texture, TexCoord) * gain * balance_effect;
    debug_audio_texture = output_audio_texture;
}