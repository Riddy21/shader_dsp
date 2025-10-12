// Inline template wrappers that delegate to non-template helpers in the .cpp.
// This lets users use any T without explicit instantiations here.

template <typename T>
bool AudioControlRegistry::set_control(const std::vector<std::string>& control_path, const T& value) {
    return set_control_any(control_path, std::any(value));
}

template <typename T>
AudioControl<T>* AudioControlRegistry::get_control(const std::vector<std::string>& control_path) {
    return dynamic_cast<AudioControl<T>*>(get_control_untyped(control_path));
}

template <typename T>
std::shared_ptr<AudioControl<T>> AudioControlRegistry::deregister_control(const std::vector<std::string>& control_path) {
    if (control_path.empty()) return nullptr;
    
    // Split path into category path and control name
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string control_name = control_path.back();
    
    auto base_ptr = deregister_control_if(
        category_path, control_name,
        [](AudioControlBase& c) { return dynamic_cast<AudioControl<T>*>(&c) != nullptr; }
    );
    if (!base_ptr) return nullptr;
    return std::static_pointer_cast<AudioControl<T>>(base_ptr);
}

// (No explicit template declarations here.)


