#include <iostream>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glut.h>
#include <cstdlib>
#include "audio_renderer.h"
#include "audio_generator_render_stage.h"
#include "audio_player_output.h"
#include "audio_file_output.h"
#include "keyboard.h"
#include "key.h"

float middle_c = 1.214879f;
float semi_tone = 1.059463f;

int main(int argc, char** argv) {
    //system("sudo renice -18 $(pgrep audio_program)");

    // Get the render program
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    Keyboard & keyboard = Keyboard::get_instance();
    std::vector<char> keys = {'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k'};
    float tone = middle_c;

    for (size_t i = 0; i < keys.size(); ++i) {
        auto key = std::make_unique<PianoKey>(keys[i], "media/sine.wav");
        key->set_gain(0.3f);
        key->set_tone(tone);
        keyboard.add_key(std::move(key));
        tone *= semi_tone;
    }

    // Make an output player
    auto audio_player_output = std::make_unique<AudioPlayerOutput>(512, 44100, 2);
    auto audio_file_output = std::make_unique<AudioFileOutput>(512, 44100, 2, "output.wav");

    // Initialize the audio renderer
    audio_renderer.initialize(512, 44100, 2);
    keyboard.initialize();

    audio_renderer.add_render_output(std::move(audio_player_output));
    audio_renderer.add_render_output(std::move(audio_file_output));

    auto player = audio_renderer.find_render_output(0);
    auto file = audio_renderer.find_render_output(1);

    audio_renderer.set_lead_output(0);

    player->open();
    player->start();
    file->open();
    file->start();

    // Start the audio renderer main loop
    audio_renderer.start_main_loop();

    player->stop();
    player->close();
    file->stop();
    file->close();

    audio_renderer.terminate();

    return 0;
}