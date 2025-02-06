#pragma once
#ifndef AUDIO_FILE_GENERATOR_RENDER_STAGE_H
#define AUDIO_FILE_GENERATOR_RENDER_STAGE_H

#include "audio_render_stage/audio_generator_render_stage.h"

/**
 * @class AudioFileGeneratorRenderStage
 * @brief The AudioFileGeneratorRenderStage class is responsible for generating audio data from a file.
 * 
 */
class AudioFileGeneratorRenderStage : public AudioGeneratorRenderStage {
public:
    /**
     * @brief Constructs an AudioFileGeneratorRenderStage object.
     * 
     * This constructor initializes the AudioFileGeneratorRenderStage object with the specified frames per buffer,
     * sample rate, number of channels, and audio file path.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of channels in the audio data.
     * @param audio_filepath The path to the audio file to use
     */
    AudioFileGeneratorRenderStage(const unsigned int frames_per_buffer,
                                  const unsigned int sample_rate,
                                  const unsigned int num_channels,
                                  const std::string& audio_filepath) ;
    /**
     * @brief Destroys the AudioFileGeneratorRenderStage object.
     */
    ~AudioFileGeneratorRenderStage() {}

    const std::string & m_audio_filepath; // Default audio file path

private:
    // Load audio file
    /**
     * @brief Loads audio data from the specified audio file.
     * 
     * This function is responsible for loading audio data from the specified audio file into the full_audio_data vector.
     */
    static std::vector<float> load_audio_data_from_file(const std::string & audio_filepath);
};

#endif // AUDIO_FILE_GENERATOR_RENDER_STAGE_H