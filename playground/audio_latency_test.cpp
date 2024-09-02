#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <cmath>

#include "audio_player_output.h"

const int frames_per_buffer = 512;
const int channels = 2;

// Generate the audio data
float audio_data_interleaved[frames_per_buffer*channels];
float silence[frames_per_buffer*channels] = {0};

AudioBuffer * audio_buffer = new AudioBuffer(1);
AudioBuffer * audio_buffer2 = new AudioBuffer(1);

// Initialize audio player output
AudioPlayerOutput audio_player_output(frames_per_buffer, 44100, channels);

void keyboard_callback(unsigned char key, int x, int y) {
    if (key == 'a') {
        audio_player_output.set_buffer_link(audio_buffer2);
    } else if (key == 's') {
        audio_player_output.set_buffer_link(audio_buffer);
    }
}

int main(int argc, char** argv) {
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Audio Latency Test");

    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    // Generate sine wave for both channels
    for (int i = 0; i < frames_per_buffer*channels; i=i+channels) {
        audio_data_interleaved[i] = sin(((double)(i)/((double)channels*(double)frames_per_buffer)) * M_PI * 10.0);
        audio_data_interleaved[i+1] = sin(((double)(i)/((double)channels*(double)frames_per_buffer)) * M_PI * 10.0);
    }

    // Create audio buffer
    audio_buffer->push(silence, frames_per_buffer*channels);
    audio_buffer2->push(audio_data_interleaved, frames_per_buffer*channels);

    // Set the audio buffer link
    audio_player_output.set_buffer_link(audio_buffer);

    // Open the audio player output
    audio_player_output.open();
    audio_player_output.start();

    glutKeyboardFunc(keyboard_callback);
    glutDisplayFunc([]() {
        glClear(GL_COLOR_BUFFER_BIT);
        glutSwapBuffers();
    });

    glutMainLoop();

    return 0;
}