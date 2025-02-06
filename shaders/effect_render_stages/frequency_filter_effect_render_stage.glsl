uniform float low_pass;
uniform float high_pass;

// Constants for our FIR kernel
const int KERNEL_SIZE = 31;   // Must be odd

//------------------------------------------------------------------------------
// A helper “sinc” function: sinc(x) = sin(πx)/(πx), with sinc(0) = 1.
float sinc(float x) {
    return abs(x) < 0.0001 ? 1.0 : sin(PI * x) / (PI * x);
}

//------------------------------------------------------------------------------
// Main – apply our FIR filter to the sample from the appropriate channel.
void main() {
    //==========================================================================
    // 1. Figure out which texel (and hence which channel) we are processing.
    //==========================================================================
    // Compute the pixel index from TexCoord.x.
    // (Assuming TexCoord.x ∈ [0,1), multiply by buffer_size.)
    float pixelPos = TexCoord.x * float(buffer_size);
    int index = int(floor(pixelPos));

    // Determine if we are in the left channel (even index) or right channel (odd index).
    bool isLeft = (index % 2 == 0);
    
    // Compute the “channel sample index” – i.e. which sample in that channel.
    // For left channel, the sample index is index/2; for right, it is (index-1)/2.
    int channelSampleIndex = isLeft ? (index / 2) : ((index - 1) / 2);
    // The number of samples per channel:
    int numChannelSamples = buffer_size / 2;
    
    //==========================================================================
    // 2. Set up our band–pass filter FIR kernel.
    //==========================================================================
    // We want to design a filter that passes frequencies between high_pass and low_pass.
    // One way is to “subtract” two ideal low–pass filters.
    // The ideal impulse response for a low–pass filter is:
    //    h_lp(m) = 2*(cutoff/sample_rate) * sinc( 2*(cutoff/sample_rate)*m )
    // For a band–pass filter we define:
    //    h_band(m) = h_lp(low_pass, m) - h_lp(high_pass, m)
    // (We center the impulse response at m=0; in our kernel m will run from -halfKernel to +halfKernel.)
    
    int halfKernel = KERNEL_SIZE / 2;
    // Precompute normalized cutoff frequencies (relative to sample rate)
    float normLow  = low_pass  / float(sample_rate);
    float normHigh = high_pass / float(sample_rate);
    
    //==========================================================================
    // 3. Run the convolution: sum over the kernel taps (only for samples in the same channel)
    //==========================================================================
    float filteredSample = 0.0;
    float kernelSum = 0.0; // for normalization
    
    // Loop over each tap in the kernel.
    // Note: GLSL ES requires the loop bounds to be constant.
    for (int n = 0; n < KERNEL_SIZE; n++) {
        // m is the relative offset (in sample units) from the center tap.
        int m = n - halfKernel;
        
        // Compute the ideal low–pass impulse responses:
        float hLowPass = 2.0 * normLow * sinc(2.0 * normLow * float(m));
        float hHighPass = 2.0 * normHigh * sinc(2.0 * normHigh * float(m));
        // Our band–pass impulse is the difference:
        float h = hLowPass - hHighPass;
        
        // Apply a Hamming window to reduce ringing:
        float window = 0.54 - 0.46 * cos(2.0 * PI * float(n) / float(KERNEL_SIZE - 1));
        h *= window;
        
        // Accumulate kernel sum (for later normalization)
        kernelSum += h;
        
        // Figure out which channel sample to use:
        int sampleIndex = channelSampleIndex + m;
        // Clamp sampleIndex to valid indices.
        sampleIndex = clamp(sampleIndex, 0, numChannelSamples - 1);
        
        // Map the channel sample index back to a texel index:
        // For left channel, samples are at even indices: texel = sampleIndex * 2.
        // For right channel, texels are at: sampleIndex * 2 + 1.
        int texelIndex = isLeft ? (sampleIndex * 2) : (sampleIndex * 2 + 1);
        
        // Convert texel index to a texture coordinate.
        // (Assume our texture width is buffer_size and use a half–texel offset.)
        float texCoordX = (float(texelIndex) + 0.5) / float(buffer_size);
        // Since our texture is 1 pixel high, we can fix y to 0.5.
        vec2 sampleTexCoord = vec2(texCoordX, 0.5);
        
        // Fetch the sample value.
        // (Assume the sample is stored in the red channel.)
        float sampleValue = texture(stream_audio_texture, sampleTexCoord).r;
        
        // Multiply the sample by the current kernel tap and add to the running sum.
        filteredSample += sampleValue * h;
    }
    
    // Normalize the filtered sample by the kernel sum so that unity gain is maintained.
    if (abs(kernelSum) > 0.0001) {
        filteredSample /= kernelSum;
    }
    
    //==========================================================================
    // 4. Output the filtered sample.
    //==========================================================================
    // (Since our output texture is the same as the input, we simply write the filtered value.
    //  In this example we output the filtered sample into the red channel.
    //  Later stages may reassemble the interleaved stereo stream.)
    output_audio_texture = vec4(filteredSample, 0.0, 0.0, 1.0);
}