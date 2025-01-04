uniform int play_position;
uniform int stop_position;
uniform bool play;
uniform float tone;
uniform float gain;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float MAX_TIME = 83880.0; // This is the maximum time in seconds before precision is lost

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
    return mod(timeVal + 3600.0, MAX_TIME);
}
