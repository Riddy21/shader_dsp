// Inline template wrappers that delegate to non-template helpers in the .cpp.
// This lets users use any T without explicit instantiations here.

template <typename T>
bool AudioControlRegistry::set_control(const std::string& name, const T& value) {
    return set_control_any(std::vector<std::string>{default_category()}, name, std::any(value));
}

template <typename T>
bool AudioControlRegistry::set_control(const std::string& category, const std::string& name, const T& value) {
    return set_control_any(std::vector<std::string>{category}, name, std::any(value));
}

template <typename T>
bool AudioControlRegistry::set_control(const std::vector<std::string>& category_path, const std::string& name, const T& value) {
    return set_control_any(category_path, name, std::any(value));
}

template <typename T>
AudioControl<T>* AudioControlRegistry::get_control(const std::string& name) {
    return dynamic_cast<AudioControl<T>*>(get_control_untyped(std::vector<std::string>{default_category()}, name));
}

template <typename T>
AudioControl<T>* AudioControlRegistry::get_control(const std::string& category, const std::string& name) {
    return dynamic_cast<AudioControl<T>*>(get_control_untyped(std::vector<std::string>{category}, name));
}

template <typename T>
AudioControl<T>* AudioControlRegistry::get_control(const std::vector<std::string>& category_path, const std::string& name) {
    return dynamic_cast<AudioControl<T>*>(get_control_untyped(category_path, name));
}

template <typename T>
std::unique_ptr<AudioControl<T>> AudioControlRegistry::deregister_control(const std::string& name) {
    auto base_ptr = deregister_control_if(
        std::vector<std::string>{default_category()}, name,
        [](AudioControlBase& c) { return dynamic_cast<AudioControl<T>*>(&c) != nullptr; }
    );
    if (!base_ptr) return nullptr;
    return std::unique_ptr<AudioControl<T>>(static_cast<AudioControl<T>*>(base_ptr.release()));
}

template <typename T>
std::unique_ptr<AudioControl<T>> AudioControlRegistry::deregister_control(const std::string& category, const std::string& name) {
    auto base_ptr = deregister_control_if(
        std::vector<std::string>{category}, name,
        [](AudioControlBase& c) { return dynamic_cast<AudioControl<T>*>(&c) != nullptr; }
    );
    if (!base_ptr) return nullptr;
    return std::unique_ptr<AudioControl<T>>(static_cast<AudioControl<T>*>(base_ptr.release()));
}

template <typename T>
std::unique_ptr<AudioControl<T>> AudioControlRegistry::deregister_control(const std::vector<std::string>& category_path, const std::string& name) {
    auto base_ptr = deregister_control_if(
        category_path, name,
        [](AudioControlBase& c) { return dynamic_cast<AudioControl<T>*>(&c) != nullptr; }
    );
    if (!base_ptr) return nullptr;
    return std::unique_ptr<AudioControl<T>>(static_cast<AudioControl<T>*>(base_ptr.release()));
}

// (No explicit template declarations here.)


