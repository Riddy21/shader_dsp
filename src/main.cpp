#include <iostream>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glut.h>
#include <cstdlib>
#include "audio_renderer.h"
#include "audio_generator_render_stage.h"
#include <audio_player_output.h>

// Function to handle key press events
void key_down_callback(unsigned char key, int x, int y) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(0);

    auto gain_param = audio_generator->find_parameter("gain");
    auto tone_param = audio_generator->find_parameter("tone");

    if (key == 'q') {
        AudioRenderer::get_instance().terminate();
    } else if (key == 'a') {
        gain_param->set_value(new float(1.0));
        tone_param->set_value(new float(0.75f));
    } else if (key == 's') {
        gain_param->set_value(new float(1.0));
        tone_param->set_value(new float(0.5f));
    }
}

// Function to handle key release events
void key_up_callback(unsigned char key, int x, int y) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(0);

    auto gain_param = audio_generator->find_parameter("gain");
    auto tone_param = audio_generator->find_parameter("tone");
    auto time_param = audio_generator->find_parameter("time");
    auto play_param = audio_generator->find_parameter("play_position");

    if (key == 'a' || key == 's') {
        play_param->set_value(time_param->get_value());
        gain_param->set_value(new float(0.0));
    }
}

int main(int argc, char** argv) {
    //system("sudo renice -18 $(pgrep audio_program)");
    // Create an audio generator render stage with sine wave
    AudioGeneratorRenderStage audio_generator(1024, 44100, 2, "media/sine.wav");

    // Get the render program
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    // Add the audio generator render stage to the audio renderer
    audio_renderer.add_render_stage(audio_generator);

    // Initialize the audio renderer
    audio_renderer.init(1024, 44100, 2);

    // Add the keyboard callback to the audio renderer
    audio_renderer.add_keyboard_down_callback(key_down_callback);
    audio_renderer.add_keyboard_up_callback(key_up_callback);

    // Make an output player
    AudioPlayerOutput audio_player_output(1024, 44100, 2);

    // Link it to the audio renderer
    auto audio_buffer = audio_renderer.get_new_output_buffer();

    // Set the buffer link
    audio_player_output.set_buffer_link(audio_buffer);

    // Start the audio player
    audio_player_output.open();
    audio_player_output.start();

    audio_buffer->push(new float[512*2]());
    audio_buffer->push(new float[512*2]());
    audio_buffer->push(new float[512*2]());
    audio_buffer->push(new float[512*2]());

    // Start the audio renderer main loop
    audio_renderer.main_loop();

    // Terminate the audio renderer
    audio_player_output.stop();
    audio_player_output.close();

    return 0;
}