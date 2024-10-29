#include <iostream>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glut.h>
#include <cstdlib>
#include "audio_renderer.h"
#include "audio_generator_render_stage.h"
#include "audio_player_output.h"
#include "keyboard.h"
#include "key.h"
#include "audio_file_output.h"

float middle_c = 1.214879f;
float semi_tone = 1.059463f;

int main(int argc, char** argv) {
    //system("sudo renice -18 $(pgrep audio_program)");

    // Get the render program
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    Keyboard & keyboard = Keyboard::get_instance();
    std::vector<char> keys = {'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k'};
    std::vector<const char *> audio_files = {"media/sine.wav", "media/test.wav", "media/fade.wav"};
    float tone = middle_c;

    for (size_t i = 0; i < keys.size(); ++i) {
        auto key = std::make_unique<PianoKey>(keys[i], audio_files[i%3]);
        key->set_gain(1.0f);
        key->set_tone(1.0f);
        keyboard.add_key(std::move(key));
        tone *= semi_tone;
    }

    // Initialize the audio renderer
    audio_renderer.init(512, 44100, 2);
    keyboard.initialize();

    // Make an output player
    AudioPlayerOutput audio_player_output(512, 44100, 2);
    //AudioFileOutput audio_file_output(512, 44100, 2, "build/tests/output.wav");

    // Link it to the audio renderer
    auto audio_buffer = audio_renderer.get_new_output_buffer();
    //auto audio_buffer2 = audio_renderer.get_new_output_buffer();

    // push some data to the audio buffer
    audio_buffer->push(new float[512*2](), true);
    //audio_buffer->push(new float[512*2](), true);

    //audio_buffer2->push(new float[512*2](), true);
    //audio_buffer2->push(new float[512*2](), true);

    printf("Starting audio player\n");

    // Set the buffer link
    audio_player_output.set_buffer_link(audio_buffer);
    //audio_file_output.set_buffer_link(audio_buffer2);

    // Start the audio player
    audio_player_output.open();
    audio_player_output.start();
    //audio_file_output.open();
    //audio_file_output.start();

    // Start the audio renderer main loop
    audio_renderer.main_loop();

    // Terminate the audio renderer
    audio_player_output.stop();
    audio_player_output.close();
    //audio_file_output.stop();
    //audio_file_output.close();

    return 0;
}