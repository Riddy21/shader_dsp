#include <iostream>
#include <unordered_map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glut.h>
#include <cstdlib>
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "audio_output/audio_file_output.h"
#include "keyboard/keyboard.h"
#include "keyboard/key.h"

float middle_c = 261.63f;
float semi_tone = 1.059463f;

const std::unordered_map<unsigned char, float> KEY_TONE_MAPPING = {
    {'a', MIDDLE_C},
    {'w', MIDDLE_C * std::pow(SEMI_TONE, 1)},
    {'s', MIDDLE_C * std::pow(SEMI_TONE, 2)},
    {'e', MIDDLE_C * std::pow(SEMI_TONE, 3)},
    {'d', MIDDLE_C * std::pow(SEMI_TONE, 4)},
    {'f', MIDDLE_C * std::pow(SEMI_TONE, 5)},
    {'t', MIDDLE_C * std::pow(SEMI_TONE, 6)},
    {'g', MIDDLE_C * std::pow(SEMI_TONE, 7)},
    {'y', MIDDLE_C * std::pow(SEMI_TONE, 8)},
    {'h', MIDDLE_C * std::pow(SEMI_TONE, 9)},
    {'u', MIDDLE_C * std::pow(SEMI_TONE, 10)},
    {'j', MIDDLE_C * std::pow(SEMI_TONE, 11)},
    {'k', MIDDLE_C * std::pow(SEMI_TONE, 12)},
};

void instantiate_piano_keyboard(Keyboard & keyboard, AudioGeneratorRenderStage & generator) {
    for (const auto & [key, tone] : KEY_TONE_MAPPING) {
        auto key_obj = new Key(key);
        key_obj->set_key_down_callback([&generator, tone]() {
            generator.play_note(tone, 0.2f);
        });
        key_obj->set_key_up_callback([&generator, tone]() {
            generator.stop_note(tone);
        });
        keyboard.add_key(key_obj);
    }
}

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

    // Add pause key
    auto pause_key = new Key('p');
    pause_key->set_key_down_callback([]() {
        AudioRenderer::get_instance().pause_main_loop();
    });
    keyboard.add_key(pause_key);

    // Add unpause key
    auto unpause_key = new Key('o');
    unpause_key->set_key_down_callback([]() {
        AudioRenderer::get_instance().unpause_main_loop();
    });
    keyboard.add_key(unpause_key);

    auto increment_key = new Key('i');
    increment_key->set_key_down_callback([]() {
        AudioRenderer::get_instance().increment_main_loop();
    });
    keyboard.add_key(increment_key);

    // add an effect render stage
    auto generator_render_stage = new AudioGeneratorRenderStage(512, 44100, 2, "build/shaders/multinote_sine_generator_render_stage.glsl");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);
    auto echo_render_stage = new AudioEchoEffectRenderStage(512, 44100, 2);
    auto filter_render_stage = new AudioFrequencyFilterEffectRenderStage(512, 44100, 2);
    auto record_render_stage = new AudioRecordRenderStage(512, 44100, 2);
    auto playback_render_stage = new AudioPlaybackRenderStage(512, 44100, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    instantiate_piano_keyboard(keyboard, *generator_render_stage);

    filter_render_stage->set_filter_follower(100.0f);

    effect_render_stage->find_parameter("gain")->set_value(1.0f);
    effect_render_stage->find_parameter("balance")->set_value(0.5f);

    generator_render_stage->connect_render_stage(effect_render_stage);
    effect_render_stage->connect_render_stage(echo_render_stage);
    echo_render_stage->connect_render_stage(filter_render_stage);
    filter_render_stage->connect_render_stage(record_render_stage);
    record_render_stage->connect_render_stage(playback_render_stage);
    playback_render_stage->connect_render_stage(final_render_stage);

    auto record_key = new Key('r');
    record_key->set_key_down_callback([&record_render_stage]() {
        if (record_render_stage->is_recording()) {
            record_render_stage->stop();
        } else {
            record_render_stage->record(0);
        }
    });
    keyboard.add_key(record_key);

    auto playback_key = new Key('l');
    playback_key->set_key_down_callback([&playback_render_stage, &record_render_stage]() {
        // Load the data
        playback_render_stage->load_tape(record_render_stage->get_tape());

        if (playback_render_stage->is_playing()) {
            playback_render_stage->stop();
        } else {
            playback_render_stage->play(0);
        }
    });
    keyboard.add_key(playback_key);

    auto filter_high_band_decrement_key = new Key('z');
    filter_high_band_decrement_key->set_key_down_callback([&filter_render_stage]() {
        filter_render_stage->set_high_pass(filter_render_stage->get_high_pass() - 10.0f);
        printf("High pass: %f\n", filter_render_stage->get_high_pass());
    });
    keyboard.add_key(filter_high_band_decrement_key);

    auto filter_high_band_increment_key = new Key('x');
    filter_high_band_increment_key->set_key_down_callback([&filter_render_stage]() {
        filter_render_stage->set_high_pass(filter_render_stage->get_high_pass() + 10.0f);
        printf("High pass: %f\n", filter_render_stage->get_high_pass());
    });
    keyboard.add_key(filter_high_band_increment_key);

    auto filter_low_band_decrement_key = new Key('c');
    filter_low_band_decrement_key->set_key_down_callback([&filter_render_stage]() {
        filter_render_stage->set_low_pass(filter_render_stage->get_low_pass() - 10.0f);
        printf("Low pass: %f\n", filter_render_stage->get_low_pass());
    });
    keyboard.add_key(filter_low_band_decrement_key);

    auto filter_low_band_increment_key = new Key('v');
    filter_low_band_increment_key->set_key_down_callback([&filter_render_stage]() {
        filter_render_stage->set_low_pass(filter_render_stage->get_low_pass() + 10.0f);
        printf("Low pass: %f\n", filter_render_stage->get_low_pass());
    });
    keyboard.add_key(filter_low_band_increment_key);

    auto audio_render_graph = new AudioRenderGraph(final_render_stage);

    audio_renderer.add_render_graph(audio_render_graph);

    // Make an output player
    auto audio_player_output = new AudioPlayerOutput(512, 44100, 2);
    auto audio_file_output = new AudioFileOutput(512, 44100, 2, "build/output.wav");

    // Initialize the audio renderer
    audio_renderer.initialize(512, 44100, 2);
    keyboard.initialize(audio_renderer.get_window());

    audio_renderer.add_render_output(audio_player_output);
    audio_renderer.add_render_output(audio_file_output);

    audio_renderer.set_lead_output(0);

    audio_player_output->open();
    audio_player_output->start();
    audio_file_output->open();
    audio_file_output->start();

    // Start the audio renderer main loop
    audio_renderer.start_main_loop();

    audio_player_output->stop();
    audio_player_output->close();
    audio_file_output->stop();
    audio_file_output->close();
    // Deallocate the audio renderer and keyboard
    keyboard.terminate();

    return 0;
}