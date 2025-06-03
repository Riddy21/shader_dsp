template <typename T>
void AudioControlRegistry::register_control(const std::string& name, AudioControl<T>* control) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_controls[name] = std::unique_ptr<AudioControlBase>(control);
}

template <typename T>
bool AudioControlRegistry::set_control(const std::string& name, const T& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_controls.find(name);
    if (it != m_controls.end()) {
        // Safe to static_cast because only AudioControl<T> is registered for this name
        auto control = static_cast<AudioControl<T>*>(it->second.get());
        control->set(value);
        return true;
    }
    return false;
}

template <typename T>
AudioControl<T>* AudioControlRegistry::get_control(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_controls.find(name);
    if (it != m_controls.end()) {
        // Safe to static_cast because only AudioControl<T> is registered for this name
        return static_cast<AudioControl<T>*>(it->second.get());
    }
    return nullptr;
}
