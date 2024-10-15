#include "audio_renderer.h"
#include "audio_generator_render_stage.h"
#include "keyboard.h"

PianoKey::PianoKey(const unsigned char key, const char * audio_file_path) : Key(key) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    auto audio_generator = std::make_unique<AudioGeneratorRenderStage>(512, 44100, 2, audio_file_path);

    //audio_renderer.add_render_stage(std::move(audio_generator));
}

void PianoKey::key_down() {
}

void PianoKey::key_up() {
}