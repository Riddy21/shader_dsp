#pragma once
#ifndef AUDIO_FILE_OUTPUT_H
#define AUDIO_FILE_OUTPUT_H

#include <fstream>
#include <string>

#include "audio_output/audio_wav.h"
#include "audio_output/audio_output.h"

class AudioFileOutput : public AudioOutput {
public:
    /**
     * Constructs an AudioFileOutput object.
     * 
     * @param filename The name of the audio file to write to.
     */
    AudioFileOutput(const unsigned frames_per_buffer,
                    const unsigned sample_rate,
                    const unsigned channels,
                    const std::string filename) : 
        AudioOutput(frames_per_buffer, sample_rate, channels),
        m_filename(filename) {}

    /**
     * Destroys the AudioFileOutput object.
     */
    ~AudioFileOutput() override;

    /**
     * Checks if the audio file is ready for writing.
     * 
     * @return True if the audio file is ready for writing, false otherwise.
     */
    bool is_ready() override;

    /**
     * Writes audio data to the file.
     * 
     * @param data The audio data to write.
     */
    void push(const float * data) override;

    /**
     * Opens the file for writing audio data.
     * 
     * @return True if the audio file was opened successfully, false otherwise.
     */
    bool open() override;

    /**
     * Starts writing audio data to the file.
     * 
     * @return True if the audio file writer was started successfully, false otherwise.
     */
    bool start() override;

    /**
     * Stops writing audio data to the file.
     * 
     * @return True if the audio file writer was stopped successfully, false otherwise.
     */
    bool stop() override;

    /**
     * Closes the audio file writer and saves the audio data.
     * 
     * @return True if the audio file writer was closed successfully, false otherwise.
     */
    bool close() override;

private:
    std::string m_filename;
    std::ofstream m_file;
    bool m_is_running = false;
    WAVHeader m_header;
};

#endif // AUDIO_FILE_OUTPUT_H