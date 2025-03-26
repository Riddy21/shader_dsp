const int maxNotes = 24; // Maximum number of simultaneous notes

uniform int play_positions[maxNotes];
uniform int stop_positions[maxNotes];
uniform float tones[maxNotes];
uniform float gains[maxNotes];
uniform int active_notes;

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

float calculateTimeSimple(int time) {
    return float(time) * float(buffer_size) / float(sample_rate);
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
