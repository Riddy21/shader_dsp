uniform int play_position;
uniform int stop_position;
uniform bool play;
uniform float tone;
uniform float gain;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float MAX_TIME = 83880.0; // This is the maximum time in seconds before precision is lost
const float MIDDLE_C = 261.63; // Middle C in Hz

float calculateTime(int time, vec2 TexCoord) {
    // Each buffer = buffer_size samples
    // Each sample = 1.0 / sample_rate seconds
    float blockDuration = float(buffer_size) / float(sample_rate);

    // Convert the current frame into a time offset, then add the fraction
    // from TexCoord.x. Example:
    //   global_time_val = which block (integer)
    //   TexCoord.x in [0..1], we multiply by blockDuration to get partial offset
    float timeVal = float(time) * blockDuration 
                  + TexCoord.x * blockDuration;

    // Keep it from becoming too large:
    return mod(timeVal, MAX_TIME);
}

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

// FIXME: Add function for interpretting larger stream texture
// FIXME: Add final render stage shader with a different sized output texture
