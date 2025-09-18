#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <csignal>

#include "audio_output/audio_wav.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_array_parameter.h"
#include "audio_render_stage/audio_generator_render_stage.h"

const std::vector<std::string> AudioSingleShaderGeneratorRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl",
    "build/shaders/generator_render_stage_settings.glsl",
    "build/shaders/adsr_envelope.glsl"
};

AudioSingleShaderGeneratorRenderStage::AudioSingleShaderGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const std::string & fragment_shader_path,
                                                     const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    auto play_position_parameter =
        new AudioIntParameter("play_position",
                              AudioParameter::ConnectionType::INPUT);
    play_position_parameter->set_value(0);

    auto stop_position_parameter =
        new AudioIntParameter("stop_position",
                              AudioParameter::ConnectionType::INPUT);
    stop_position_parameter->set_value(-1);

    auto play_parameter =
        new AudioBoolParameter("play",
                              AudioParameter::ConnectionType::INPUT);
    play_parameter->set_value(false);

    auto tone_parameter =
        new AudioFloatParameter("tone",
                              AudioParameter::ConnectionType::INPUT);
    tone_parameter->set_value(MIDDLE_C);

    auto gain_parameter =
        new AudioFloatParameter("gain",
                              AudioParameter::ConnectionType::INPUT);
    gain_parameter->set_value(1.0f);

    if (!this->add_parameter(play_parameter)) {
        std::cerr << "Failed to add play_parameter" << std::endl;
    }
    if (!this->add_parameter(tone_parameter)) {
        std::cerr << "Failed to add tone_parameter" << std::endl;
    }
    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(play_position_parameter)) {
        std::cerr << "Failed to add play_position_parameter" << std::endl;
    }
    if (!this->add_parameter(stop_position_parameter)) {
        std::cerr << "Failed to add stop_position_parameter" << std::endl;
    }

    // TODO: Envelope Parameters, Consider moving to a separate class
    auto attack_time_parameter =
        new AudioFloatParameter("attack_time",
                              AudioParameter::ConnectionType::INPUT);
    attack_time_parameter->set_value(0.1f);

    auto decay_time_parameter =
        new AudioFloatParameter("decay_time",
                              AudioParameter::ConnectionType::INPUT);
    decay_time_parameter->set_value(1.0f);

    auto sustain_level_parameter =
        new AudioFloatParameter("sustain_level",
                              AudioParameter::ConnectionType::INPUT);
    sustain_level_parameter->set_value(1.0f);

    auto release_time_parameter =
        new AudioFloatParameter("release_time",
                              AudioParameter::ConnectionType::INPUT);
    release_time_parameter->set_value(0.4f);

    if (!this->add_parameter(attack_time_parameter)) {
        std::cerr << "Failed to add attack_time_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_time_parameter)) {
        std::cerr << "Failed to add decay_time_parameter" << std::endl;
    }
    if (!this->add_parameter(sustain_level_parameter)) {
        std::cerr << "Failed to add sustain_level_parameter" << std::endl;
    }
    if (!this->add_parameter(release_time_parameter)) {
        std::cerr << "Failed to add release_time_parameter" << std::endl;
    }
}

// String-based constructor for AudioSingleShaderGeneratorRenderStage
AudioSingleShaderGeneratorRenderStage::AudioSingleShaderGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const std::string & fragment_shader_source,
                                                     bool use_shader_string,
                                                     const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_source, use_shader_string, frag_shader_imports) {

    auto play_position_parameter =
        new AudioIntParameter("play_position",
                              AudioParameter::ConnectionType::INPUT);
    play_position_parameter->set_value(0);

    auto stop_position_parameter =
        new AudioIntParameter("stop_position",
                              AudioParameter::ConnectionType::INPUT);
    stop_position_parameter->set_value(-1);

    auto play_parameter =
        new AudioBoolParameter("play",
                              AudioParameter::ConnectionType::INPUT);
    play_parameter->set_value(false);

    auto tone_parameter =
        new AudioFloatParameter("tone",
                              AudioParameter::ConnectionType::INPUT);
    tone_parameter->set_value(MIDDLE_C);

    auto gain_parameter =
        new AudioFloatParameter("gain",
                              AudioParameter::ConnectionType::INPUT);
    gain_parameter->set_value(1.0f);

    if (!this->add_parameter(play_parameter)) {
        std::cerr << "Failed to add play_parameter" << std::endl;
    }
    if (!this->add_parameter(tone_parameter)) {
        std::cerr << "Failed to add tone_parameter" << std::endl;
    }
    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(play_position_parameter)) {
        std::cerr << "Failed to add play_position_parameter" << std::endl;
    }
    if (!this->add_parameter(stop_position_parameter)) {
        std::cerr << "Failed to add stop_position_parameter" << std::endl;
    }

    // TODO: Envelope Parameters, Consider moving to a separate class
    auto attack_time_parameter =
        new AudioFloatParameter("attack_time",
                              AudioParameter::ConnectionType::INPUT);
    attack_time_parameter->set_value(0.1f);

    auto decay_time_parameter =
        new AudioFloatParameter("decay_time",
                              AudioParameter::ConnectionType::INPUT);
    decay_time_parameter->set_value(1.0f);

    auto sustain_level_parameter =
        new AudioFloatParameter("sustain_level",
                              AudioParameter::ConnectionType::INPUT);
    sustain_level_parameter->set_value(1.0f);

    auto release_time_parameter =
        new AudioFloatParameter("release_time",
                              AudioParameter::ConnectionType::INPUT);
    release_time_parameter->set_value(0.4f);

    if (!this->add_parameter(attack_time_parameter)) {
        std::cerr << "Failed to add attack_time_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_time_parameter)) {
        std::cerr << "Failed to add decay_time_parameter" << std::endl;
    }
    if (!this->add_parameter(sustain_level_parameter)) {
        std::cerr << "Failed to add sustain_level_parameter" << std::endl;
    }
    if (!this->add_parameter(release_time_parameter)) {
        std::cerr << "Failed to add release_time_parameter" << std::endl;
    }
}

const std::vector<std::string> AudioGeneratorRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl",
    "build/shaders/multinote_generator_render_stage_settings.glsl",
    "build/shaders/adsr_envelope.glsl"
};

AudioGeneratorRenderStage::AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const std::string & fragment_shader_path,
                                                     const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports),
      m_note_state(MAX_NOTES_PLAYED_AT_ONCE) // initialize NoteState
{

    auto play_position_parameter =
        new AudioIntArrayParameter("play_positions",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto stop_position_parameter =
        new AudioIntArrayParameter("stop_positions",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto tone_parameter =
        new AudioFloatArrayParameter("tones",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);
    
    auto gain_parameter =
        new AudioFloatArrayParameter("gains",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto active_notes_parameter =
        new AudioIntParameter("active_notes",
                              AudioParameter::ConnectionType::INPUT);
                        
    // Set to 0s
    int* play_positions = new int[MAX_NOTES_PLAYED_AT_ONCE]();
    int* stop_positions = new int[MAX_NOTES_PLAYED_AT_ONCE]();
    float* tones = new float[MAX_NOTES_PLAYED_AT_ONCE]();
    float* gains = new float[MAX_NOTES_PLAYED_AT_ONCE]();

    play_position_parameter->set_value(play_positions);
    stop_position_parameter->set_value(stop_positions);
    tone_parameter->set_value(tones);
    gain_parameter->set_value(gains);
    active_notes_parameter->set_value(0);

    // Clean up allocated arrays
    delete[] play_positions;
    delete[] stop_positions;
    delete[] tones;
    delete[] gains;

    // Add the parameters
    if (!this->add_parameter(play_position_parameter)) {
        std::cerr << "Failed to add play_position_parameter" << std::endl;
    }
    if (!this->add_parameter(stop_position_parameter)) {
        std::cerr << "Failed to add stop_position_parameter" << std::endl;
    }
    if (!this->add_parameter(tone_parameter)) {
        std::cerr << "Failed to add tone_parameter" << std::endl;
    }
    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(active_notes_parameter)) {
        std::cerr << "Failed to add active_notes_parameter" << std::endl;
    }

    auto attack_time_parameter =
        new AudioFloatParameter("attack_time",
                              AudioParameter::ConnectionType::INPUT);
    attack_time_parameter->set_value(0.1f);

    auto decay_time_parameter =
        new AudioFloatParameter("decay_time",
                              AudioParameter::ConnectionType::INPUT);
    decay_time_parameter->set_value(1.0f);

    auto sustain_level_parameter =
        new AudioFloatParameter("sustain_level",
                              AudioParameter::ConnectionType::INPUT);
    sustain_level_parameter->set_value(1.0f);

    auto release_time_parameter =
        new AudioFloatParameter("release_time",
                              AudioParameter::ConnectionType::INPUT);
    release_time_parameter->set_value(0.4f);

    if (!this->add_parameter(attack_time_parameter)) {
        std::cerr << "Failed to add attack_time_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_time_parameter)) {
        std::cerr << "Failed to add decay_time_parameter" << std::endl;
    }
    if (!this->add_parameter(sustain_level_parameter)) {
        std::cerr << "Failed to add sustain_level_parameter" << std::endl;
    }
    if (!this->add_parameter(release_time_parameter)) {
        std::cerr << "Failed to add release_time_parameter" << std::endl;
    }

    // Register controls
    m_controls.clear();

    auto * attack_time_control = new AudioControl<float>("attack_time", *(float*)attack_time_parameter->get_value(), [attack_time_parameter](const float& v) { attack_time_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("attack_time", attack_time_control);
    m_controls.push_back(attack_time_control);

    auto * decay_time_control = new AudioControl<float>("decay_time", *(float*)decay_time_parameter->get_value(), [decay_time_parameter](const float& v) { decay_time_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("decay_time", decay_time_control);
    m_controls.push_back(decay_time_control);

    auto * sustain_level_control = new AudioControl<float>("sustain_level", *(float*)sustain_level_parameter->get_value(), [sustain_level_parameter](const float& v) { sustain_level_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("sustain_level", sustain_level_control);
    m_controls.push_back(sustain_level_control);

    auto * release_time_control = new AudioControl<float>("release_time", *(float*)release_time_parameter->get_value(), [release_time_parameter](const float& v) { release_time_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("release_time", release_time_control);
    m_controls.push_back(release_time_control);

    // Play notes parameter
    auto * play_note_control = new AudioControl<std::pair<float, float>>("play_note", std::make_pair(440.0f, 1.0f), [this](const std::pair<float, float>& v) { play_note(v); });
    AudioControlRegistry::instance().register_control("play_note", play_note_control);
    m_controls.push_back(play_note_control);

    auto * stop_note_control = new AudioControl<float>("stop_note", 0.0f, [this](const float& v) { stop_note(v); });
    AudioControlRegistry::instance().register_control("stop_note", stop_note_control);
    m_controls.push_back(stop_note_control);
}

// String-based constructor for AudioGeneratorRenderStage
AudioGeneratorRenderStage::AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const std::string & fragment_shader_source,
                                                     bool use_shader_string,
                                                     const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_source, use_shader_string, frag_shader_imports),
      m_note_state(MAX_NOTES_PLAYED_AT_ONCE) // initialize NoteState
{

    auto play_position_parameter =
        new AudioIntArrayParameter("play_positions",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto stop_position_parameter =
        new AudioIntArrayParameter("stop_positions",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto tone_parameter =
        new AudioFloatArrayParameter("tones",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);
    
    auto gain_parameter =
        new AudioFloatArrayParameter("gains",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto active_notes_parameter =
        new AudioIntParameter("active_notes",
                              AudioParameter::ConnectionType::INPUT);
                        
    // Set to 0s
    int* play_positions = new int[MAX_NOTES_PLAYED_AT_ONCE]();
    int* stop_positions = new int[MAX_NOTES_PLAYED_AT_ONCE]();
    float* tones = new float[MAX_NOTES_PLAYED_AT_ONCE]();
    float* gains = new float[MAX_NOTES_PLAYED_AT_ONCE]();

    play_position_parameter->set_value(play_positions);
    stop_position_parameter->set_value(stop_positions);
    tone_parameter->set_value(tones);
    gain_parameter->set_value(gains);
    active_notes_parameter->set_value(0);

    // Clean up allocated arrays
    delete[] play_positions;
    delete[] stop_positions;
    delete[] tones;
    delete[] gains;

    // Add the parameters
    if (!this->add_parameter(play_position_parameter)) {
        std::cerr << "Failed to add play_position_parameter" << std::endl;
    }
    if (!this->add_parameter(stop_position_parameter)) {
        std::cerr << "Failed to add stop_position_parameter" << std::endl;
    }
    if (!this->add_parameter(tone_parameter)) {
        std::cerr << "Failed to add tone_parameter" << std::endl;
    }
    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(active_notes_parameter)) {
        std::cerr << "Failed to add active_notes_parameter" << std::endl;
    }

    auto attack_time_parameter =
        new AudioFloatParameter("attack_time",
                              AudioParameter::ConnectionType::INPUT);
    attack_time_parameter->set_value(0.1f);

    auto decay_time_parameter =
        new AudioFloatParameter("decay_time",
                              AudioParameter::ConnectionType::INPUT);
    decay_time_parameter->set_value(1.0f);

    auto sustain_level_parameter =
        new AudioFloatParameter("sustain_level",
                              AudioParameter::ConnectionType::INPUT);
    sustain_level_parameter->set_value(1.0f);

    auto release_time_parameter =
        new AudioFloatParameter("release_time",
                              AudioParameter::ConnectionType::INPUT);
    release_time_parameter->set_value(0.4f);

    if (!this->add_parameter(attack_time_parameter)) {
        std::cerr << "Failed to add attack_time_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_time_parameter)) {
        std::cerr << "Failed to add decay_time_parameter" << std::endl;
    }
    if (!this->add_parameter(sustain_level_parameter)) {
        std::cerr << "Failed to add sustain_level_parameter" << std::endl;
    }
    if (!this->add_parameter(release_time_parameter)) {
        std::cerr << "Failed to add release_time_parameter" << std::endl;
    }

    // Register controls
    m_controls.clear();

    auto * attack_time_control = new AudioControl<float>("attack_time", *(float*)attack_time_parameter->get_value(), [attack_time_parameter](const float& v) { attack_time_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("attack_time", attack_time_control);
    m_controls.push_back(attack_time_control);

    auto * decay_time_control = new AudioControl<float>("decay_time", *(float*)decay_time_parameter->get_value(), [decay_time_parameter](const float& v) { decay_time_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("decay_time", decay_time_control);
    m_controls.push_back(decay_time_control);

    auto * sustain_level_control = new AudioControl<float>("sustain_level", *(float*)sustain_level_parameter->get_value(), [sustain_level_parameter](const float& v) { sustain_level_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("sustain_level", sustain_level_control);
    m_controls.push_back(sustain_level_control);

    auto * release_time_control = new AudioControl<float>("release_time", *(float*)release_time_parameter->get_value(), [release_time_parameter](const float& v) { release_time_parameter->set_value(v); });
    AudioControlRegistry::instance().register_control("release_time", release_time_control);
    m_controls.push_back(release_time_control);

    // Play notes parameter
    auto * play_note_control = new AudioControl<std::pair<float, float>>("play_note", std::make_pair(440.0f, 1.0f), [this](const std::pair<float, float>& v) { play_note(v); });
    AudioControlRegistry::instance().register_control("play_note", play_note_control);
    m_controls.push_back(play_note_control);

    auto * stop_note_control = new AudioControl<float>("stop_note", 0.0f, [this](const float& v) { stop_note(v); });
    AudioControlRegistry::instance().register_control("stop_note", stop_note_control);
    m_controls.push_back(stop_note_control);
}

void AudioGeneratorRenderStage::play_note(const std::pair<float, float>& note)
{
    float tone = note.first;
    float gain = note.second;

    unsigned int time;
    if (m_time == 0) time = m_time;
    else time = m_time + 1;

    unsigned int idx = m_note_state.add_note(time, -1, tone, gain, MAX_NOTES_PLAYED_AT_ONCE); // Shift by one because note doesn't start until next frame

    if (idx >= MAX_NOTES_PLAYED_AT_ONCE) {
        delete_note(0);
    }

    // Update the shader parameters
    m_note_state.set_parameters(this);
}

void AudioGeneratorRenderStage::stop_note(const float tone)
{
    unsigned int time;
    if (m_time == 0) time = m_time;
    else time = m_time + 1;

    int tone_index = m_note_state.stop_note(tone, time);

    if (tone_index == -1) {
        return;
    }

    auto stop_positions = find_parameter("stop_positions");
    if (stop_positions) stop_positions->set_value(m_note_state.m_stop_positions.data());

    static float last_release_time = -1.0f;
    static int release_time_buffers = 0;

    // TODO: abstract this so that you can apply to other envelopes
    float release_time = *(float *)find_parameter("release_time")->get_value();
    if (release_time != last_release_time) {
        float seconds_per_buffer = (float)frames_per_buffer / (float)sample_rate;
        release_time_buffers = (int)(release_time / seconds_per_buffer) + 1;
        last_release_time = release_time;
    }

    m_delete_at_time[m_time + release_time_buffers] = tone_index;
}

void AudioGeneratorRenderStage::delete_note(const unsigned int index)
{
    m_note_state.delete_note(index);

    for (auto & [delete_time, delete_index] : m_delete_at_time) {
        if (delete_index >= index) {
            m_delete_at_time[delete_time] = delete_index - 1;
        }
    }

    // Update the shader parameters
    m_note_state.set_parameters(this);
}

// TODO: Consolidate int time passed in and global time variable into one single variable
void AudioGeneratorRenderStage::render(const unsigned int time) {
    AudioRenderStage::render(time);

    if (m_delete_at_time.find(time) != m_delete_at_time.end()) {
        delete_note(m_delete_at_time[time]);
        m_delete_at_time.erase(time);
    }
}

bool AudioGeneratorRenderStage::connect_render_stage(AudioRenderStage * next_stage) {
    if (!AudioRenderStage::connect_render_stage(next_stage)) {
        return false;
    }

    NoteState::download_clipboard(m_note_state);
    m_note_state.set_parameters(this);
    printf("Downloaded note state from clipboard with %d active notes.\n", m_note_state.m_active_notes);

    return true;
}

bool AudioGeneratorRenderStage::disconnect_render_stage(AudioRenderStage * next_stage) {
    if (!AudioRenderStage::disconnect_render_stage(next_stage)) {
        return false;
    }

    NoteState::upload_clipboard(m_note_state);
    m_note_state.clear();
    m_note_state.set_parameters(this);
    printf("Copied note state to clipboard with %d active notes.\n", m_note_state.m_active_notes);

    return true;
}

// --- NoteState Implementation ---

AudioGeneratorRenderStage::NoteState::NoteState(unsigned int max_notes)
    : m_active_notes(0),
      m_play_positions(max_notes, 0),
      m_stop_positions(max_notes, 0),
      m_tones(max_notes, 0.f),
      m_gains(max_notes, 0.f)
{}

void AudioGeneratorRenderStage::NoteState::set_parameters(AudioGeneratorRenderStage* owner) {
    auto active_notes = owner->find_parameter("active_notes");
    auto play_positions = owner->find_parameter("play_positions");
    auto stop_positions = owner->find_parameter("stop_positions");
    auto tones = owner->find_parameter("tones");
    auto gains = owner->find_parameter("gains");
    if (active_notes) active_notes->set_value(m_active_notes);
    if (play_positions) play_positions->set_value(m_play_positions.data());
    if (stop_positions) stop_positions->set_value(m_stop_positions.data());
    if (tones) tones->set_value(m_tones.data());
    if (gains) gains->set_value(m_gains.data());
}

void AudioGeneratorRenderStage::NoteState::copy_from(const NoteState& other) {
    m_active_notes = other.m_active_notes;
    m_play_positions = other.m_play_positions;
    m_stop_positions = other.m_stop_positions;
    m_tones = other.m_tones;
    m_gains = other.m_gains;
}

unsigned int AudioGeneratorRenderStage::NoteState::add_note(int play_position, int stop_position, float tone, float gain, unsigned int max_notes) {
    if (m_active_notes >= max_notes) return max_notes; // signal overflow
    m_play_positions[m_active_notes] = play_position;
    m_stop_positions[m_active_notes] = stop_position;
    m_tones[m_active_notes] = tone;
    m_gains[m_active_notes] = gain;
    return m_active_notes++;
}

void AudioGeneratorRenderStage::NoteState::delete_note(unsigned int index) {
    if (index >= m_active_notes) return;
    for (unsigned int i = index; i < m_active_notes - 1; i++) {
        m_play_positions[i] = m_play_positions[i + 1];
        m_stop_positions[i] = m_stop_positions[i + 1];
        m_tones[i] = m_tones[i + 1];
        m_gains[i] = m_gains[i + 1];
    }
    m_active_notes--;
}

int AudioGeneratorRenderStage::NoteState::stop_note(float tone, int stop_time) {
    for (unsigned int i = 0; i < m_active_notes; i++) {
        if (m_tones[i] == tone && m_stop_positions[i] == -1) {
            m_stop_positions[i] = stop_time;
            return static_cast<int>(i);
        }
    }
    return -1;
}

// --- NoteState static clipboard ---
AudioGeneratorRenderStage::NoteState* AudioGeneratorRenderStage::NoteState::clipboard = nullptr;

// Singleton instance, ensures clipboard is cleared at program exit
AudioGeneratorRenderStage::NoteState::ClipboardCleaner AudioGeneratorRenderStage::NoteState::clipboard_cleaner_instance;

// ClipboardCleaner constructor (does nothing)
AudioGeneratorRenderStage::NoteState::ClipboardCleaner::ClipboardCleaner() {}

// ClipboardCleaner destructor: clears clipboard at program exit
AudioGeneratorRenderStage::NoteState::ClipboardCleaner::~ClipboardCleaner() {
    // This will be called once at program exit, ensuring clipboard is wiped exactly once.
    AudioGeneratorRenderStage::NoteState::clear_clipboard();
}

void AudioGeneratorRenderStage::NoteState::clear() {
    m_active_notes = 0;
    std::fill(m_play_positions.begin(), m_play_positions.end(), 0);
    std::fill(m_stop_positions.begin(), m_stop_positions.end(), 0);
    std::fill(m_tones.begin(), m_tones.end(), 0.f);
    std::fill(m_gains.begin(), m_gains.end(), 0.f);
}

void AudioGeneratorRenderStage::NoteState::upload_clipboard(NoteState& src) {
    if (clipboard) {
        delete clipboard;
        clipboard = nullptr;
    }
    clipboard = new NoteState(src);
}

bool AudioGeneratorRenderStage::NoteState::download_clipboard(NoteState& dst) {
    if (!clipboard) return false;
    dst.copy_from(*clipboard);
    return true;
}

void AudioGeneratorRenderStage::NoteState::clear_clipboard() {
    if (clipboard) {
        delete clipboard;
        clipboard = nullptr;
    }
}