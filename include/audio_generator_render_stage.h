#pragma once
#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#define MAX_TEXTURE_SIZE 4096

#include "audio_render_stage.h"

/**
 * @class AudioGenerator
 * @brief The AudioGenerator class is responsible for generating audio data for the shader DSP.
 *
 * The AudioGenerator class provides functionality to generate audio data for the shader DSP. It
 * manages the audio buffer, sample rate, and audio data size. Subclasses can override the
 * `load_audio_data` function to load specific audio data.
 */
class AudioGeneratorRenderStage : public AudioRenderStage {
public:
    /**
     * @brief Constructs an AudioGenerator object.
     * 
     * This constructor initializes the AudioGenerator object with the specified frames per buffer,
     * sample rate, number of channels, and audio file path.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of channels in the audio data.
     * @param audio_filepath The path to the audio file to use
     */
    AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                              const unsigned int sample_rate,
                              const unsigned int num_channels,
                              const char * audio_filepath);

    /**
     * @brief Destroys the AudioGenerator object.
     */
    ~AudioGeneratorRenderStage() {} // TODO: Implement destructor

    const char * m_audio_filepath; // Default audio file path

    /**
     * @brief Overrides the get_fragment_source function to provide the fragment shader source code.
     * 
     * This function returns the fragment shader source code as a string.
     * 
     * @return The fragment shader source code.
     */
    virtual const GLchar* get_fragment_source() const override {
        return m_fragment_source;
    }

private:
    // Load audio file
    /**
     * @brief Loads audio data from the specified audio file.
     * 
     * This function is responsible for loading audio data from the specified audio file into the full_audio_data vector.
     */
    static std::vector<float> load_audio_data_from_file(const char * audio_filepath);

    const GLchar * m_fragment_source = R"glsl(
        #version 300 es
        precision highp float;
        
        in vec2 TexCoord;
        
        uniform sampler2D full_audio_data_texture;
        
        layout(std140) uniform TimeBuffer {
            int time;
        };
        
        out vec4 output_audio_texture;

        vec2 chunkCoord(sampler2D source, vec2 coord) {
            // Define the size of the audio data and the chunk
            ivec2 audio_size = textureSize(source, 0);
            ivec2 chunk_size = ivec2(1024, 1);
            // Calculate the number of chunks in the audio data
            ivec2 num_chunks = audio_size / chunk_size;
            // Determine the current chunk based on the frame number (time)
            int current_chunk_index = time % (num_chunks.x * num_chunks.y);
            // Calculate the coordinates of the current chunk
            ivec2 chunk_coord = ivec2(current_chunk_index % num_chunks.x, current_chunk_index / num_chunks.x);
            // Map the texture coordinates to the current chunk
            return coord * vec2(chunk_size) / vec2(audio_size) + vec2(chunk_coord) * vec2(chunk_size) / vec2(audio_size);
        }

        void main() {
            vec2 coordinates = chunkCoord(full_audio_data_texture, TexCoord);
        
            // Sample the texture and output the result
            output_audio_texture = texture(full_audio_data_texture, coordinates);
        }
    )glsl";

    // controls
    unsigned int m_play_index = 0;
};

#endif