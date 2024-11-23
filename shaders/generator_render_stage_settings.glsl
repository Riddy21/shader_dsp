uniform int play_position;
uniform int stop_position;
uniform bool play;
uniform float tone;
uniform float gain; // FIXME: Move gain to effect when ready

float calculateTime(int time, vec2 TexCoord, int buffer_size) {
    // FIXME: Change 44100 to a default audio parameter
    return mod((float(time) + TexCoord.x) / (44100.0 / float(buffer_size)), 83880.0);
}

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float MAX_TIME = 83880.0; // This is the maximum time in seconds before precision is lost

