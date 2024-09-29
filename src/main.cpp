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

    if (key == 'q') {
        AudioRenderer::get_instance().terminate();
    } else if (key == 'a') {
        AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(0);

        auto gain_param = audio_generator->find_parameter("gain");
        auto tone_param = audio_generator->find_parameter("tone");
        auto play_param = audio_generator->find_parameter("play_position");
        auto time_param = audio_generator->find_parameter("time");
        play_param->set_value(time_param->get_value());
        gain_param->set_value(new float(0.3));
        tone_param->set_value(new float(0.5f));
    } else if (key == 's') {
        AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(1);

        auto gain_param = audio_generator->find_parameter("gain");
        auto tone_param = audio_generator->find_parameter("tone");
        auto play_param = audio_generator->find_parameter("play_position");
        auto time_param = audio_generator->find_parameter("time");
        play_param->set_value(time_param->get_value());
        gain_param->set_value(new float(0.3));
        tone_param->set_value(new float(1.1f));
    } else if (key == 'd') {
        AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(2);

        auto gain_param = audio_generator->find_parameter("gain");
        auto tone_param = audio_generator->find_parameter("tone");
        auto play_param = audio_generator->find_parameter("play_position");
        auto time_param = audio_generator->find_parameter("time");
        play_param->set_value(time_param->get_value());
        gain_param->set_value(new float(0.3));
        tone_param->set_value(new float(1.5f));
    }
}

// Function to handle key release events
void key_up_callback(unsigned char key, int x, int y) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    if (key == 'a') {
        AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(0);

        auto gain_param = audio_generator->find_parameter("gain");
        auto tone_param = audio_generator->find_parameter("tone");
        gain_param->set_value(new float(0.0));
        tone_param->set_value(new float(0.0f));
    } else if (key == 's') {
        AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(1);

        auto gain_param = audio_generator->find_parameter("gain");
        auto tone_param = audio_generator->find_parameter("tone");
        gain_param->set_value(new float(0.0));
        tone_param->set_value(new float(0.0f));
    } else if (key == 'd') {
        AudioGeneratorRenderStage * audio_generator = (AudioGeneratorRenderStage *)audio_renderer.get_render_stage(2);

        auto gain_param = audio_generator->find_parameter("gain");
        auto tone_param = audio_generator->find_parameter("tone");
        gain_param->set_value(new float(0.0));
        tone_param->set_value(new float(0.0f));
    }

}

int main(int argc, char** argv) {
    //system("sudo renice -18 $(pgrep audio_program)");
    // Create an audio generator render stage with sine wave
    AudioGeneratorRenderStage key_a(512, 44100, 2, "media/sine.wav");
    AudioGeneratorRenderStage key_s(512, 44100, 2, "media/sine.wav");
    AudioGeneratorRenderStage key_d(512, 44100, 2, "media/sine.wav");

    // Link render stages
    auto output_audio_texture = key_a.find_parameter("output_audio_texture");
    auto stream_audio_texture = key_s.find_parameter("stream_audio_texture");
    auto output_audio_texture2 = key_s.find_parameter("output_audio_texture");
    auto stream_audio_texture2 = key_d.find_parameter("stream_audio_texture");

    output_audio_texture->link(stream_audio_texture);
    output_audio_texture2->link(stream_audio_texture2);

    // Get the render program
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    // Add the audio generator render stage to the audio renderer
    audio_renderer.add_render_stage(key_a);
    audio_renderer.add_render_stage(key_s);
    audio_renderer.add_render_stage(key_d);

    // Initialize the audio renderer
    audio_renderer.init(512, 44100, 2);

    // Add the keyboard callback to the audio renderer
    audio_renderer.add_keyboard_down_callback(key_down_callback);
    audio_renderer.add_keyboard_up_callback(key_up_callback);

    // Make an output player
    AudioPlayerOutput audio_player_output(512, 44100, 2);

    // Link it to the audio renderer
    auto audio_buffer = audio_renderer.get_new_output_buffer();

    // Set the buffer link
    audio_player_output.set_buffer_link(audio_buffer);

    // Start the audio player
    audio_player_output.open();
    audio_player_output.start();

    // Start the audio renderer main loop
    audio_renderer.main_loop();

    // Terminate the audio renderer
    audio_player_output.stop();
    audio_player_output.close();

    return 0;
}