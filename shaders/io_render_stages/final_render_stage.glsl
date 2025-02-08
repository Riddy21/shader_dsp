layout(location = 1) out vec4 final_output_audio_texture;

void main(){
    // Get the texture dimensions (this works for any texture size)
    ivec2 dims = textureSize(stream_audio_texture, 0);
    float width  = float(dims.x);
    float height = float(dims.y);
    
    // Determine our output pixel’s integer coordinates.
    // (Assuming TexCoord is in [0,1] the pixel coordinate is floor(TexCoord * dims).)
    vec2 outPixel = floor(TexCoord * vec2(width, height));
    
    // Compute the output’s linear index (row–major order):
    float k = outPixel.y * width + outPixel.x;
    
    // Invert the mapping:  k = (width - 1 - j)*height + i
    // so that:
    float i = mod(k, height);              // the input row
    float j = width - 1.0 - floor(k / height); // the input column
    
    // Now convert the input pixel coordinate (j,i) back into normalized texture coordinates.
    vec2 inPixel = vec2(j, i);
    vec2 inTexCoord = (inPixel + 0.5) / vec2(width, height);
    
    // Finally, sample the input texture at the rotated coordinate.
    final_output_audio_texture = texture(stream_audio_texture, inTexCoord);
    output_audio_texture = texture(stream_audio_texture, inTexCoord);
}
