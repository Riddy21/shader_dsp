#include <thread>
#include <GL/glew.h>
#include <GL/glut.h>
#include <chrono>

#include "audio_renderer.h"
#include "audio_generator_render_stage.h"
#include "audio_player_output.h"

int main() {
    AudioGeneratorRenderStage audio_generator(512, 44100, 2, "media/test.wav");
    AudioPlayerOutput audio_driver(512, 44100, 2);
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    audio_renderer.add_render_stage(audio_generator);

    auto play_param = audio_generator.find_parameter("gain");
    auto tone_param = audio_generator.find_parameter("tone");
    auto position_param = audio_generator.find_parameter("play_position");

    // Start the audio driver
    audio_renderer.init(512, 44100, 2);
    auto audio_buffer = audio_renderer.get_new_output_buffer();
    audio_driver.set_buffer_link(audio_buffer);
    audio_driver.open();
    audio_driver.start();

    // Buffer some data
    audio_buffer->push(new float[512*2]());

    // Open a thread to wait few sec and the shut it down
    play_param->set_value(new float(0.8f));
    tone_param->set_value(new float(0.8f));

    audio_renderer.main_loop();

    return 0;
}