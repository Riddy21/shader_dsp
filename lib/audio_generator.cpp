#include <cmath>
#include <GL/glew.h>

#include "audio_generator.h"

AudioGenerator::AudioGenerator(const unsigned audio_data_size, const unsigned sample_rate) :
        sample_rate(sample_rate),
        audio_data_size(audio_data_size),
        audio_data_left(audio_data_size),
        audio_data_right(audio_data_size) {
    // prepare the PBO for writing audio data to the texture
    setup_audio_buffer();
}

AudioGenerator::~AudioGenerator() {
}

bool AudioGenerator::update_audio_buffer()
{
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, input_pixel_buffer);
    // TODO: Starting here, implement the update_audio_buffer function
    float * ptr = (float *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

    // FIXME: think of a way to avoid this copy

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return false;
}

void AudioGenerator::load_audio_data() {
    // TODO: Add tone control
    // Generate sine wave for both channels
    for (unsigned i = 0; i < audio_data_size; i++) {
        audio_data_left[i] = sin(((double)i/(double)audio_data_size) * M_PI * 10.0);
        audio_data_right[i] = sin(((double)i/(double)audio_data_size) * M_PI * 10.0);
    }
}

bool AudioGenerator::setup_audio_buffer() {
    glGenBuffers(1, &input_pixel_buffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, input_pixel_buffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, audio_data_size * sizeof(float), NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        return false;
    }
    return true;
}