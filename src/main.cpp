#include <iostream>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glut.h>
#include <cstdlib>
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_gain_effect_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
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
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    keyboard.get_output_render_stage()->connect_render_stage(effect_render_stage);
    effect_render_stage->connect_render_stage(final_render_stage);

    auto audio_render_graph = new AudioRenderGraph(final_render_stage);

    audio_renderer.add_render_graph(audio_render_graph);

    // Make an output player
    auto audio_player_output = new AudioPlayerOutput(512, 44100, 2);
    auto audio_file_output = new AudioFileOutput(512, 44100, 2, "build/output.wav");

    // Initialize the audio renderer
    audio_renderer.initialize(512, 44100, 2);
    keyboard.initialize();

    audio_renderer.add_render_output(audio_player_output);
    audio_renderer.add_render_output(audio_file_output);

    audio_renderer.set_lead_output(0);

    audio_player_output->open();
    audio_player_output->start();
    audio_file_output->open();
    audio_file_output->start();

    // get the render stage 
    auto gain_param = effect_render_stage->find_parameter("gain");
    gain_param->set_value(1.0f);
    auto balance_param = effect_render_stage->find_parameter("balance");
    balance_param->set_value(1.0f);

    // Start the audio renderer main loop
    audio_renderer.start_main_loop();

    audio_player_output->stop();
    audio_player_output->close();
    audio_file_output->stop();
    audio_file_output->close();

    return 0;
}