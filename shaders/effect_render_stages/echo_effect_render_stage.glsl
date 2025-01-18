uniform int num_echos;
uniform float delay;
uniform float decay;
uniform sampler2D echo_audio_texture;

void main() {
    vec4 yo = texture(echo_audio_texture, vec2(TexCoord.x, 1.0));
    vec4 yo2 = texture(echo_audio_texture, vec2(TexCoord.x, 0.5));
    output_audio_texture = texture(stream_audio_texture, TexCoord) + yo + yo2;
}