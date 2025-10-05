#include "audio_core/audio_control.h"

// Global registry for controls
AudioControlRegistry& AudioControlRegistry::instance() {
    static AudioControlRegistry inst;
    return inst;
}

const std::string& AudioControlRegistry::default_category() {
    static const std::string kDefaultCategory = "default";
    return kDefaultCategory;
}

std::vector<std::string> AudioControlRegistry::list_controls() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    std::function<void(const CategoryNode&)> dfs = [&](const CategoryNode& node) {
        for (const auto& kv : node.controls) names.push_back(kv.first);
        for (const auto& child : node.children) dfs(*child.second);
    };
    dfs(m_root);
    return names;
}

std::vector<std::string> AudioControlRegistry::list_controls(const std::string& category) {
    // Delegate to vector overload which handles locking
    return list_controls(std::vector<std::string>{category});
}

std::vector<std::string> AudioControlRegistry::list_categories() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> cats;
    for (const auto& kv : m_root.children) cats.push_back(kv.first);
    return cats;
}

void AudioControlRegistry::register_control(const std::string& name, AudioControlBase* control) {
    register_control(std::vector<std::string>{default_category()}, name, control);
}

void AudioControlRegistry::register_control(const std::string& category, const std::string& name, AudioControlBase* control) {
    register_control(std::vector<std::string>{category}, name, control);
}

AudioControlRegistry::AudioControlRegistry() = default;

AudioControlRegistry::CategoryNode* AudioControlRegistry::navigate_to_node_unlocked(const std::vector<std::string>& category_path, bool create_if_missing) {
    CategoryNode* current = &m_root;
    for (const std::string& part : category_path) {
        auto it = current->children.find(part);
        if (it == current->children.end()) {
            if (!create_if_missing) return nullptr;
            current->children[part] = std::unique_ptr<CategoryNode>(new CategoryNode());
            it = current->children.find(part);
        }
        current = it->second.get();
    }
    return current;
}

void AudioControlRegistry::register_control(const std::vector<std::string>& category_path, const std::string& name, AudioControlBase* control) {
    std::lock_guard<std::mutex> lock(m_mutex);
    CategoryNode* node = navigate_to_node_unlocked(category_path, true);
    if (node->controls.find(name) != node->controls.end()) {
        throw std::runtime_error("Control already registered at path with name: " + name);
    }
    node->controls[name] = std::unique_ptr<AudioControlBase>(control);
}

std::vector<std::string> AudioControlRegistry::list_controls(const std::vector<std::string>& category_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    CategoryNode* node = navigate_to_node_unlocked(category_path, false);
    if (!node) return names;
    for (const auto& kv : node->controls) names.push_back(kv.first);
    return names;
}

std::vector<std::string> AudioControlRegistry::list_categories(const std::vector<std::string>& category_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> cats;
    CategoryNode* node = navigate_to_node_unlocked(category_path, false);
    if (!node) return cats;
    for (const auto& kv : node->children) cats.push_back(kv.first);
    return cats;
}

// Non-template helper implementations used by template wrappers in the .tpp

bool AudioControlRegistry::set_control_any(const std::vector<std::string>& category_path, const std::string& name, const std::any& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    CategoryNode* node = navigate_to_node_unlocked(category_path, false);
    if (!node) return false;
    auto it = node->controls.find(name);
    if (it == node->controls.end()) return false;
    try {
        it->second->set_value(value);
        return true;
    } catch (...) {
        return false;
    }
}

AudioControlBase* AudioControlRegistry::get_control_untyped(const std::vector<std::string>& category_path, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    CategoryNode* node = navigate_to_node_unlocked(category_path, false);
    if (!node) return nullptr;
    auto it = node->controls.find(name);
    if (it == node->controls.end()) return nullptr;
    return it->second.get();
}

std::unique_ptr<AudioControlBase> AudioControlRegistry::deregister_control_if(
    const std::vector<std::string>& category_path,
    const std::string& name,
    const std::function<bool(AudioControlBase&)>& predicate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    CategoryNode* node = navigate_to_node_unlocked(category_path, false);
    if (!node) return nullptr;
    auto it = node->controls.find(name);
    if (it == node->controls.end()) return nullptr;
    if (!predicate(*it->second)) return nullptr;
    std::unique_ptr<AudioControlBase> base_ptr = std::move(it->second);
    node->controls.erase(it);
    return base_ptr;
}