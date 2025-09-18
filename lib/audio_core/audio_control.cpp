#include "audio_core/audio_control.h"

// Global registry for controls
AudioControlRegistry& AudioControlRegistry::instance() {
    static AudioControlRegistry inst;
    return inst;
}

std::vector<std::string> AudioControlRegistry::list_controls() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& kv : m_controls) names.push_back(kv.first);
    return names;
}

void AudioControlRegistry::register_control(const std::string& name, AudioControlBase* control) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_controls[name] = std::unique_ptr<AudioControlBase>(control);
}

AudioControlRegistry::AudioControlRegistry() = default;