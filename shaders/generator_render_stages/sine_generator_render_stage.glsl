float calculatePhase(int time, vec2 TexCoord, float tone) {
    // How many total samples have elapsed up to this frame?
    //   total_samples = frame * bufSize + fractional_samples
    // Where fractional_samples = texCoord.x * bufSize
    // (TexCoord.x goes from 0 to 1, corresponding to sub-frame offset).
    int totalSamples = time * buffer_size + int(TexCoord.x * float(buffer_size));

    // Keep it from growing too large by modding with sampleRate
    // so the maximum value of totalSamples is < sampleRate.
    // This effectively gives you a [0,1) oscillator in time.
    int modSamples = totalSamples % int(float(sample_rate) / tone);

    // Convert to float time in [0,1).
    return float(modSamples) / float(sample_rate);
}

float generateSine(float tone) {
    float phase = calculatePhase(global_time_val, TexCoord, tone);

    return sin(TWO_PI * tone * phase);
}

void main() {
    float start_time = calculateTime(play_position, vec2(0.0, 0.0));
    float end_time = calculateTime(stop_position, vec2(0.0, 0.0));
    float time = calculateTime(global_time_val, TexCoord);
    float sine_out = generateSine(tone) * adsr_envelope(start_time, end_time, time);
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(sine_out * gain, 0.0, 0.0, 0.0);
}

