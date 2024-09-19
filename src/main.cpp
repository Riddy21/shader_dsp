#include <iostream>
#include <cmath>
#include <vector>

#include "audio_renderer.h"
#include "audio_player_output.h"
#include "audio_generator_render_stage.h"

//#define M_PI 3.14159265358979323846

// Function to handle keyboard input
void key_callback(unsigned char key, int x, int y) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(0);

    auto play_param = audio_generator->find_parameter("play");
    auto tone_param = audio_generator->find_parameter("tone");

    if (key == 'q') {
        AudioRenderer::get_instance().terminate();
    } else if (key == 'a') {
        play_param->set_value(new int(1));
        tone_param->set_value(new float(0.75f));
    } else if (key == 's') {
        play_param->set_value(new int(1));
        tone_param->set_value(new float(0.5f));
    }
}

int main() {
    // Create an audio generator render stage with sine wave
    AudioGeneratorRenderStage audio_generator(512, 44100, 2, "media/sine.wav");

    // Get the render program
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    // Add the audio generator render stage to the audio renderer
    audio_renderer.add_render_stage(audio_generator);

    // Initialize the audio renderer
    audio_renderer.init(512, 44100, 2);

    // Add the keyboard callback to the audio renderer
    audio_renderer.add_keyboard_callback(key_callback);

    // Make an output player
    AudioPlayerOutput audio_player_output(512, 44100, 2);

    // Link it to the audio renderer
    auto audio_buffer = audio_renderer.get_new_output_buffer();
    //audio_buffer->push(new float[512*2](), 512*2);
    audio_buffer->push(new float[512*2](), 512*2);
    audio_player_output.set_buffer_link(audio_buffer);

    // Start the audio player
    audio_player_output.open();
    audio_player_output.start();

    // start the audio renderer main loop
    audio_renderer.main_loop();

    // Terminate the audio renderer
    audio_player_output.stop();
    audio_player_output.close();
    
    return 0;
}