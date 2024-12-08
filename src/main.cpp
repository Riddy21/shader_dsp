#include <iostream>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glut.h>
#include <cstdlib>
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "audio_output/audio_file_output.h"
#include "keyboard/keyboard.h"
#include "keyboard/key.h"

float middle_c = 261.63f;
//float middle_c = 1.214879f;
float semi_tone = 1.059463f;

int main(int argc, char** argv) {
    //system("sudo renice -18 $(pgrep audio_program)");

    // Get the render program
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    Keyboard & keyboard = Keyboard::get_instance();

    // Add a quit key
    auto quit_key = new Key('q');
    quit_key->set_key_down_callback([]() {
        AudioRenderer::get_instance().terminate();
    });
    keyboard.add_key(quit_key);

    // add an effect render stage
    auto effect_render_stage = new AudioEffectRenderStage(512, 44100, 2);

    // Make an output player
    auto audio_player_output = new AudioPlayerOutput(512, 44100, 2);
    auto audio_file_output = new AudioFileOutput(512, 44100, 2, "build/output.wav");

    // Initialize the audio renderer
    audio_renderer.initialize(512, 44100, 2);
    keyboard.initialize();

    audio_renderer.add_render_output(audio_player_output);
    audio_renderer.add_render_output(audio_file_output);

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

    return 0;
}