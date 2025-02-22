uniform float low_pass;
uniform float high_pass;

const int order = 4;

// Apply a butterworth filter to the audio
void main() {
    float nyquist = 0.5 * float(sample_rate);
    float low_pass_freq = low_pass / nyquist;
    float high_pass_freq = high_pass / nyquist;

    // Calculate coefficients for the butterworth filter
    float tan_low = tan(PI * low_pass_freq);
    float tan_high = tan(PI * high_pass_freq);
    float tan_band = tan_high - tan_low;
    float vol = pow(tan_band, float(order));

    output_audio_texture = texture(stream_audio_texture, TexCoord) * vol;
}