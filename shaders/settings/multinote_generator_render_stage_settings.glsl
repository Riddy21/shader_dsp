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
    // Use floating-point arithmetic throughout to avoid rounding errors
    // Calculate total time in seconds
    float totalTime = float(time) * float(buffer_size) / float(sample_rate) 
                    + TexCoord.x * float(buffer_size) / float(sample_rate);
    
    // Calculate the period of the tone
    float period = 1.0 / tone;
    
    // Use floating-point modulo to get the phase within one period
    // This avoids integer truncation that causes discontinuities
    // Return the phase in time units (seconds) as expected by sine generation
    return mod(totalTime, period);
}
