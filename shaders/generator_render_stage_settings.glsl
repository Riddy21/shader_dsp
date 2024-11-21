uniform int play_position;

uniform float tone;

uniform float gain;

uniform int buffer_size;

float calculateTime(int elapsed_time, vec2 TexCoord, int buffer_size) {
    // FIXME: Change 44100 to a default audio parameter
    return mod((float(elapsed_time) + TexCoord.x) / (44100.0 / float(buffer_size)), 2.0 * 3.14159265359 * tone);
}
