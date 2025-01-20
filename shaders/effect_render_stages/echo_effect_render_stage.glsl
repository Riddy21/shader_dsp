uniform int num_echos;
uniform float delay;
uniform float decay;
uniform sampler2D echo_audio_texture;

void main() {
    vec4 echo = texture(echo_audio_texture, vec2(TexCoord.x, 1.0));
    output_audio_texture = texture(stream_audio_texture, TexCoord) + echo;
}