#include <memory>
#include "audio_synthesizer/audio_track.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_synthesizer/audio_module.h"

AudioTrack::AudioTrack(AudioRenderGraph * render_graph, AudioRenderStage * root_stage,
                       const unsigned int buffer_size,
                       const unsigned int sample_rate,
                       const unsigned int num_channels) :
    m_render_graph(render_graph),
    m_audio_renderer(&AudioRenderer::get_instance()),
    m_root_stage(root_stage),
    m_buffer_size(buffer_size),
    m_sample_rate(sample_rate),
    m_num_channels(num_channels),
    m_module_manager(*render_graph, root_stage->gid, *m_audio_renderer)
{
    // Check if the render graph is initialized
    if (!m_render_graph->is_initialized()) {
        std::cerr << "Render graph is not initialized." << std::endl;
        return;
    }

    initialize_modules();

    m_current_effect = m_effect_modules["none"];
    m_current_voice = m_voice_modules["sine"];

    // Add default modules to the manager (voice first, then effect)
    m_module_manager.add_module(m_current_effect);
    m_module_manager.add_module(m_current_voice);
}

void AudioTrack::initialize_modules() {
    m_audio_renderer->activate_render_context();
    // TODO: Change this to be data driven
    // Effect modules
    auto gain_stage = new AudioGainEffectRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    gain_stage->initialize(); // FIXME: Initilize should be removed once moved to construction is initialization
    m_effect_modules["gain"] = std::make_shared<AudioEffectModule>(
        "gain",
        std::vector<AudioEffectRenderStage*>{gain_stage}
    );

    auto echo_stage = new AudioEchoEffectRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    echo_stage->initialize();
    m_effect_modules["echo"] = std::make_shared<AudioEffectModule>(
        "echo",
        std::vector<AudioEffectRenderStage*>{echo_stage}
    );

    auto frequency_filter_stage = new AudioFrequencyFilterEffectRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    frequency_filter_stage->initialize();
    m_effect_modules["frequency_filter"] = std::make_shared<AudioEffectModule>(
        "frequency_filter",
        std::vector<AudioEffectRenderStage*>{frequency_filter_stage}
    );

    auto none_stage = new AudioEffectRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    none_stage->initialize();
    m_effect_modules["none"] = std::make_shared<AudioEffectModule>(
        "none",
        std::vector<AudioEffectRenderStage*>{none_stage}
    );

    // Voice modules
    auto sine_stage = new AudioGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sine_generator_render_stage.glsl");
    sine_stage->initialize();
    m_voice_modules["sine"] = std::make_shared<AudioVoiceModule>(
        "sine",
        sine_stage
    );

    auto saw_stage = new AudioGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sawtooth_generator_render_stage.glsl");
    saw_stage->initialize();
    m_voice_modules["saw"] = std::make_shared<AudioVoiceModule>(
        "saw",
        saw_stage
    );

    auto square_stage = new AudioGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_square_generator_render_stage.glsl");
    square_stage->initialize();
    m_voice_modules["square"] = std::make_shared<AudioVoiceModule>(
        "square",
        square_stage
    );

    auto triangle_stage = new AudioGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_triangle_generator_render_stage.glsl");
    triangle_stage->initialize();
    m_voice_modules["triangle"] = std::make_shared<AudioVoiceModule>(
        "triangle",
        triangle_stage
    );

    auto file_stage = new AudioFileGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "media/test.wav");
    file_stage->initialize();
    m_voice_modules["file"] = std::make_shared<AudioVoiceModule>(
        "file",
        file_stage
    );

    m_audio_renderer->unactivate_render_context();
}

void AudioTrack::play_note(const float tone, const float gain) {
    m_current_voice->play_note(tone, gain);
}

void AudioTrack::stop_note(const float tone) {
    m_current_voice->stop_note(tone);
}

void AudioTrack::change_effect(const std::string & effect_name) {
    m_audio_renderer->activate_render_context();

    if (m_effect_modules.find(effect_name) != m_effect_modules.end()) {
        m_module_manager.replace_module(m_current_effect->name(), m_effect_modules[effect_name]);
        m_current_effect = m_effect_modules[effect_name];

        std::cout << "Switched effect to " << effect_name << std::endl;
    } else {
        std::cerr << "Effect not found: " << effect_name << std::endl;
    }

    m_audio_renderer->unactivate_render_context();
}

void AudioTrack::change_voice(const std::string & voice_name) {
    m_audio_renderer->activate_render_context();

    if (m_voice_modules.find(voice_name) != m_voice_modules.end()) {
        m_module_manager.replace_module(m_current_voice->name(), m_voice_modules[voice_name]);
        m_current_voice = m_voice_modules[voice_name];

        std::cout << "Switched voice to " << voice_name << std::endl;
    } else {
        std::cerr << "Voice not found: " << voice_name << std::endl;
    }

    m_audio_renderer->unactivate_render_context();
}

AudioTrack::~AudioTrack() {
    // Destructor implementation
}

const std::vector<std::string> AudioTrack::get_effect_names() const {
    std::vector<std::string> effect_names;
    for (const auto & effect : m_effect_modules) {
        effect_names.push_back(effect.first);
    }
    return effect_names;
}
const std::unordered_map<std::string, std::shared_ptr<AudioEffectModule>> & AudioTrack::get_effects() const {
    return m_effect_modules;
}

const std::vector<std::string> AudioTrack::get_generator_names() const {
    std::vector<std::string> generator_names;
    for (const auto & generator : m_voice_modules) {
        generator_names.push_back(generator.first);
    }
    return generator_names;
}
const std::unordered_map<std::string, std::shared_ptr<AudioVoiceModule>> & AudioTrack::get_voices() const {
    return m_voice_modules;
}