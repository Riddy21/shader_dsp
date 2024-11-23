
float generateNoise() {
    return fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    float noise_out = generateNoise();
    output_audio_texture = texture(stream_audio_texture, TexCoord) + vec4(noise_out * gain, 0.0, 0.0, 0.0);
}