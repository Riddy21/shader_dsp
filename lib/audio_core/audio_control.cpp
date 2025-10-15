#include "audio_core/audio_control.h"
#include <stack>
#include <typeinfo>
#include <typeindex>

// Global registry for controls
AudioControlRegistry& AudioControlRegistry::instance() {
    static AudioControlRegistry inst;
    return inst;
}

const std::string& AudioControlRegistry::default_category() {
    static const std::string kDefaultCategory = "default";
    return kDefaultCategory;
}

std::vector<std::string> AudioControlRegistry::list_controls(const std::optional<std::vector<std::string>>& category_path) {
    CategoryNode* node;
    if (!category_path.has_value()) {
        node = &m_root;
    } else {
        node = navigate_to_node(category_path.value(), false);
    }

    if (!node) {
        return {};
    }

    std::vector<std::string> control_names;
    for (const auto& kv : node->controls) {
        control_names.push_back(kv.first);
    }
    for (const auto& kv : node->children) {
        control_names.push_back(kv.first);
    }
    return control_names;
}

std::vector<std::string> AudioControlRegistry::list_all_controls() {
    std::vector<std::string> all_controls;

    struct StackItem {
        CategoryNode* node;
        std::vector<std::string> path;
    };

    std::stack<StackItem> stack;
    stack.push({&m_root, {}});

    while (!stack.empty()) {
        StackItem item = stack.top();
        stack.pop();
        CategoryNode* node = item.node;
        std::vector<std::string> current_path = item.path;

        for (const auto& kv : node->controls) {
            std::vector<std::string> control_path = current_path;
            control_path.push_back(kv.first);
            std::string full_path;
            for (size_t i = 0; i < control_path.size(); ++i) {
                if (i > 0) full_path += "/";
                full_path += control_path[i];
            }
            all_controls.push_back(full_path);
        }

        for (auto& child_kv : node->children) {
            std::vector<std::string> child_path = current_path;
            child_path.push_back(child_kv.first);
            stack.push({&child_kv.second, child_path});
        }
    }

    return all_controls;
}

// Update register_control for by-value ControlHandle stored in map
void AudioControlRegistry::register_control(const std::vector<std::string>& control_path, std::shared_ptr<AudioControlBase> control) {
    if (control_path.empty()) {
        throw std::runtime_error("Control path cannot be empty");
    }
    
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string control_name = control_path.back();
    
    CategoryNode* node = navigate_to_node(category_path, true);
    auto it = node->controls.find(control_name);
    if (it != node->controls.end()) {
        // Preserve previous control target so external references remain valid until unused
        if (it->second.m_control) {
            m_retired_controls.push_back(std::move(it->second.m_control));
        }
        it->second.m_control = std::move(control);
    } else {
        node->controls.emplace(control_name, ControlHandle(std::move(control)));
    }
}

// Update get_control to return a reference to the stored ControlHandle
ControlHandle & AudioControlRegistry::get_control(const std::vector<std::string>& control_path) {
    static ControlHandle s_null_handle; // empty handle for not-found cases
    if (control_path.empty()) return s_null_handle;
    
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string control_name = control_path.back();
    
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return s_null_handle;
    
    auto it = node->controls.find(control_name);
    if (it == node->controls.end()) return s_null_handle;
    return it->second;
}

// Update deregister_control_if for by-value ControlHandle
std::shared_ptr<AudioControlBase> AudioControlRegistry::deregister_control_if(
    const std::vector<std::string>& category_path,
    const std::string& name,
    const std::function<bool(AudioControlBase&)>& predicate) {
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return nullptr;
    
    auto it = node->controls.find(name);
    if (it == node->controls.end()) return nullptr;
    
    auto current = it->second.get();
    if (!current || !predicate(*current)) return nullptr;
    
    std::shared_ptr<AudioControlBase> base_ptr = std::move(it->second.m_control);
    node->controls.erase(it);
    return base_ptr;
}

std::shared_ptr<AudioControlBase> AudioControlRegistry::deregister_control(const std::vector<std::string>& control_path) {
    if (control_path.empty()) return nullptr;
    
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string name = control_path.back();
    
    return deregister_control_if(category_path, name, [](AudioControlBase&){ return true; });
}

// Fix navigate_to_node for map<CategoryNode>
AudioControlRegistry::CategoryNode* AudioControlRegistry::navigate_to_node(const std::vector<std::string>& category_path, bool create_if_missing) {
    CategoryNode* current = &m_root;
    for (const std::string& part : category_path) {
        auto it = current->children.find(part);
        if (it == current->children.end()) {
            if (!create_if_missing) return nullptr;
            current->children.emplace(part, CategoryNode());
            it = current->children.find(part);
        }
        current = &it->second;
    }
    return current;
}

AudioControlRegistry::AudioControlRegistry() = default;