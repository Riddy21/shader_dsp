#pragma once
#ifndef AUDIO_FILE_GENERATOR_RENDER_STAGE_H
#define AUDIO_FILE_GENERATOR_RENDER_STAGE_H

#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"
#include <memory>

/**
 * @class AudioFileGeneratorRenderStageBase
 * @brief The AudioFileGeneratorRenderStageBase class is a base class for generating audio data from a file.
 * 
 */
class AudioFileGeneratorRenderStageBase {
public:
    /**
     * @brief Constructs an AudioFileGeneratorRenderStageBase object.
     * 
     * This constructor initializes the AudioFileGeneratorRenderStageBase object with the specified audio file path.
     * 
     * @param audio_filepath The path to the audio file to use
     */
    AudioFileGeneratorRenderStageBase(const std::string& audio_filepath)
        : m_audio_filepath(audio_filepath) {}

    /**
     * @brief Destroys the AudioFileGeneratorRenderStageBase object.
     */
    virtual ~AudioFileGeneratorRenderStageBase() {}

protected:
    const std::string & m_audio_filepath; // Default audio file path

    // Load audio file
    /**
     * @brief Loads audio data from the specified audio file.
     * 
     * This function is responsible for loading audio data from the specified audio file into the full_audio_data vector.
     */
    static const std::vector<float> load_audio_data_from_file(const std::string & audio_filepath);
};

/**
 * @class AudioSingleShaderFileGeneratorRenderStage
 * @brief The AudioSingleShaderFileGeneratorRenderStage class is responsible for generating audio data from a file.
 * 
 */
class AudioSingleShaderFileGeneratorRenderStage : public AudioSingleShaderGeneratorRenderStage, public AudioFileGeneratorRenderStageBase {
public:
    /**
     * @brief Constructs an AudioSingleShaderFileGeneratorRenderStage object.
     * 
     * This constructor initializes the AudioSingleShaderFileGeneratorRenderStage object with the specified frames per buffer,
     * sample rate, number of channels, and audio file path.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of channels in the audio data.
     * @param audio_filepath The path to the audio file to use
     */
    AudioSingleShaderFileGeneratorRenderStage(const unsigned int frames_per_buffer,
                                  const unsigned int sample_rate,
                                  const unsigned int num_channels,
                                  const std::string& audio_filepath);

    // Named constructor
    AudioSingleShaderFileGeneratorRenderStage(const std::string & stage_name,
                                  const unsigned int frames_per_buffer,
                                  const unsigned int sample_rate,
                                  const unsigned int num_channels,
                                  const std::string& audio_filepath);
    /**
     * @brief Destroys the AudioSingleShaderFileGeneratorRenderStage object.
     */
    ~AudioSingleShaderFileGeneratorRenderStage() {}

protected:
    void render(const unsigned int time) override;

private:
    std::shared_ptr<AudioTape> m_tape;
    std::unique_ptr<AudioRenderStageHistory2> m_history2;
};

/**
 * @class AudioFileGeneratorRenderStage
 * @brief The AudioFileGeneratorRenderStage class is responsible for generating audio data from a file.
 * 
 */
class AudioFileGeneratorRenderStage : public AudioGeneratorRenderStage, public AudioFileGeneratorRenderStageBase {
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
                                  const std::string& audio_filepath);

    // Named constructor
    AudioFileGeneratorRenderStage(const std::string & stage_name,
                                  const unsigned int frames_per_buffer,
                                  const unsigned int sample_rate,
                                  const unsigned int num_channels,
                                  const std::string& audio_filepath);
    /**
     * @brief Destroys the AudioFileGeneratorRenderStage object.
     */
    ~AudioFileGeneratorRenderStage() {}

protected:
    void render(const unsigned int time) override;

private:
    std::shared_ptr<AudioTape> m_tape;
    std::unique_ptr<AudioRenderStageHistory2> m_history2;
};

#endif // AUDIO_FILE_GENERATOR_RENDER_STAGE_H