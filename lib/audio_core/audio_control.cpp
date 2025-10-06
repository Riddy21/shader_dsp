#include "audio_core/audio_control.h"
#include <stack>

// Global registry for controls
AudioControlRegistry& AudioControlRegistry::instance() {
    static AudioControlRegistry inst;
    return inst;
}

const std::string& AudioControlRegistry::default_category() {
    static const std::string kDefaultCategory = "default";
    return kDefaultCategory;
}

std::vector<std::vector<std::string>> AudioControlRegistry::list_controls() {
    std::vector<std::vector<std::string>> control_paths;
    std::stack<std::pair<const CategoryNode*, std::vector<std::string>>> stk;
    stk.push({&m_root, {}});
    while (!stk.empty()) {
        auto [node, current_path] = stk.top(); stk.pop();
        for (const auto& kv : node->controls) {
            auto path = current_path; path.push_back(kv.first);
            control_paths.push_back(path);
        }
        for (const auto& child : node->children) {
            auto path = current_path; path.push_back(child.first);
            stk.push({child.second.get(), path});
        }
    }
    return control_paths;
}

std::vector<std::vector<std::string>> AudioControlRegistry::list_controls(const std::vector<std::string>& category_path) {
    std::vector<std::vector<std::string>> control_paths;
    
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return control_paths;
    
    for (const auto& kv : node->controls) {
        std::vector<std::string> control_path = category_path;
        control_path.push_back(kv.first);
        control_paths.push_back(control_path);
    }
    // Also include controls reachable via control aliases that exist at this node (names only)
    for (const auto& alias : node->control_aliases) {
        std::vector<std::string> control_path = category_path;
        control_path.push_back(alias.first);
        control_paths.push_back(control_path);
    }
    
    return control_paths;
}

std::vector<std::vector<std::string>> AudioControlRegistry::list_categories() {
    std::vector<std::vector<std::string>> category_paths;
    std::stack<std::pair<const CategoryNode*, std::vector<std::string>>> stk;
    stk.push({&m_root, {}});
    while (!stk.empty()) {
        auto [node, current_path] = stk.top(); stk.pop();
        for (const auto& child : node->children) {
            auto path = current_path; path.push_back(child.first);
            category_paths.push_back(path);
            stk.push({child.second.get(), path});
        }
    }
    return category_paths;
}

std::vector<std::vector<std::string>> AudioControlRegistry::list_categories(const std::vector<std::string>& category_path) {
    std::vector<std::vector<std::string>> category_paths;
    
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return category_paths;
    
    for (const auto& child : node->children) {
        std::vector<std::string> child_path = category_path;
        child_path.push_back(child.first);
        category_paths.push_back(child_path);
    }
    // Include category references as categories
    for (const auto& ref : node->category_references) {
        std::vector<std::string> ref_path = category_path;
        ref_path.push_back(ref.first);
        category_paths.push_back(ref_path);
    }
    
    return category_paths;
}

void AudioControlRegistry::register_control(const std::vector<std::string>& control_path, AudioControlBase* control) {
    if (control_path.empty()) {
        throw std::runtime_error("Control path cannot be empty");
    }
    
    // Split path into category path and control name
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string control_name = control_path.back();
    
    CategoryNode* node = navigate_to_node(category_path, true);
    if (node->controls.find(control_name) != node->controls.end()) {
        throw std::runtime_error("Control already registered at path: " + control_name);
    }
    node->controls[control_name] = std::unique_ptr<AudioControlBase>(control);
}

AudioControlRegistry::AudioControlRegistry() = default;

AudioControlRegistry::CategoryNode* AudioControlRegistry::navigate_to_node(const std::vector<std::string>& category_path, bool create_if_missing) {
    CategoryNode* current = &m_root;
    for (const std::string& part : category_path) {
        auto it = current->children.find(part);
        if (it == current->children.end()) {
            // Try resolve via category alias at this level
            auto alias_it = current->category_references.find(part);
            if (alias_it != current->category_references.end() && alias_it->second != nullptr) {
                current = alias_it->second;
                continue;
            }
            if (!create_if_missing) return nullptr;
            current->children[part] = std::unique_ptr<CategoryNode>(new CategoryNode());
            it = current->children.find(part);
        }
        current = it->second.get();
    }
    return current;
}

// Non-template helper implementations used by template wrappers in the .tpp

bool AudioControlRegistry::set_control_any(const std::vector<std::string>& control_path, const std::any& value) {
    if (control_path.empty()) return false;
    
    // Split path into category path and control name
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string control_name = control_path.back();
    
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return false;
    
    AudioControlBase* target_control = nullptr;
    
    // Control alias resolution takes precedence
    auto alias_it = node->control_aliases.find(control_name);
    if (alias_it != node->control_aliases.end()) {
        CategoryNode* ref_node = alias_it->second.first;
        const std::string& ref_name = alias_it->second.second;
        if (!ref_node) return false;
        auto control_it = ref_node->controls.find(ref_name);
        if (control_it != ref_node->controls.end()) {
            target_control = control_it->second.get();
        } else {
            return false;
        }
    } else {
        // Category reference: lookup control with same name at referenced category
        auto cat_ref_it = node->category_references.find(control_name);
        if (cat_ref_it != node->category_references.end()) {
            CategoryNode* ref_node = cat_ref_it->second;
            if (!ref_node) return false;
            auto control_it = ref_node->controls.find(control_name);
            if (control_it != ref_node->controls.end()) {
                target_control = control_it->second.get();
            } else {
                return false;
            }
        } else {
            // Direct control
            auto it = node->controls.find(control_name);
            if (it == node->controls.end()) return false;
            target_control = it->second.get();
        }
    }
    
    if (!target_control) return false;
    
    try {
        target_control->set_value(value);
        return true;
    } catch (...) {
        return false;
    }
}

AudioControlBase* AudioControlRegistry::get_control_untyped(const std::vector<std::string>& control_path) {
    if (control_path.empty()) return nullptr;
    
    // Split path into category path and control name
    std::vector<std::string> category_path(control_path.begin(), control_path.end() - 1);
    std::string control_name = control_path.back();
    
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return nullptr;
    
    // Control alias
    auto alias_it = node->control_aliases.find(control_name);
    if (alias_it != node->control_aliases.end()) {
        CategoryNode* ref_node = alias_it->second.first;
        const std::string& ref_name = alias_it->second.second;
        if (!ref_node) return nullptr;
        auto control_it = ref_node->controls.find(ref_name);
        if (control_it != ref_node->controls.end()) {
            return control_it->second.get();
        }
        return nullptr;
    }
    
    // Category reference: same-name control in referenced category
    auto cat_ref_it = node->category_references.find(control_name);
    if (cat_ref_it != node->category_references.end()) {
        CategoryNode* ref_node = cat_ref_it->second;
        if (!ref_node) return nullptr;
        auto control_it = ref_node->controls.find(control_name);
        if (control_it != ref_node->controls.end()) {
            return control_it->second.get();
        }
        return nullptr;
    }
    
    // Direct control
    auto it = node->controls.find(control_name);
    if (it == node->controls.end()) return nullptr;
    return it->second.get();
}

std::unique_ptr<AudioControlBase> AudioControlRegistry::deregister_control_if(
    const std::vector<std::string>& category_path,
    const std::string& name,
    const std::function<bool(AudioControlBase&)>& predicate) {
    CategoryNode* node = navigate_to_node(category_path, false);
    if (!node) return nullptr;
    
    // Check if this is an alias - aliases cannot be deregistered via control deregistration
    if (node->category_references.find(name) != node->category_references.end()) {
        return nullptr;
    }
    if (node->control_aliases.find(name) != node->control_aliases.end()) {
        return nullptr;
    }
    
    auto it = node->controls.find(name);
    if (it == node->controls.end()) return nullptr;
    if (!predicate(*it->second)) return nullptr;
    std::unique_ptr<AudioControlBase> base_ptr = std::move(it->second);
    node->controls.erase(it);
    return base_ptr;
}

// Control reference management implementations

void AudioControlRegistry::link_control(const std::vector<std::string>& reference_control_path, const std::vector<std::string>& target_control_path) {
    if (!link_control_untyped(reference_control_path, target_control_path)) {
        throw std::runtime_error("Failed to link control reference to target control path");
    }
}

bool AudioControlRegistry::unlink_control(const std::vector<std::string>& reference_control_path) {
    return unlink_control_untyped(reference_control_path);
}

bool AudioControlRegistry::link_control_untyped(const std::vector<std::string>& reference_control_path, const std::vector<std::string>& target_control_path) {
    if (reference_control_path.empty() || target_control_path.empty()) return false;
    
    CategoryNode* target_node = nullptr;
    std::string target_control_name;
    if (target_control_path.size() == 1) {
        // Ambiguous but treat as category name under root
        target_node = navigate_to_node({}, false);
        if (!target_node) return false;
        auto it = target_node->children.find(target_control_path[0]);
        if (it == target_node->children.end()) return false;
        target_node = it->second.get();
    } else {
        // Try interpret as category path only (link to category) if the exact path corresponds to an existing category
        CategoryNode* category_candidate = navigate_to_node(target_control_path, false);
        if (category_candidate != nullptr) {
            target_node = category_candidate;
        } else {
            // Otherwise, treat last element as control name at parent category
            std::vector<std::string> target_category_path(target_control_path.begin(), target_control_path.end() - 1);
            target_node = navigate_to_node(target_category_path, false);
            if (!target_node) return false;
            target_control_name = target_control_path.back();
            // Validate control exists
            if (target_node->controls.find(target_control_name) == target_node->controls.end()) return false;
        }
    }
    
    // Find or create the reference node
    std::vector<std::string> reference_category_path(reference_control_path.begin(), reference_control_path.end() - 1);
    CategoryNode* ref_node = navigate_to_node(reference_category_path, true);
    if (!ref_node) return false;
    
    std::string reference_name = reference_control_path.back();
    
    // Ensure we don't overwrite existing direct control
    if (ref_node->controls.find(reference_name) != ref_node->controls.end()) {
        return false; // Cannot create alias with same name as existing control
    }

    // Clear any previous alias state for this name
    ref_node->category_references.erase(reference_name);
    ref_node->control_aliases.erase(reference_name);

    if (target_control_name.empty()) {
        // Category alias
        ref_node->category_references[reference_name] = target_node;
    } else {
        // Control alias to a specific control name
        ref_node->control_aliases[reference_name] = std::make_pair(target_node, target_control_name);
    }
    return true;
}

bool AudioControlRegistry::unlink_control_untyped(const std::vector<std::string>& reference_control_path) {
    if (reference_control_path.empty()) return false;
    
    std::vector<std::string> reference_category_path(reference_control_path.begin(), reference_control_path.end() - 1);
    CategoryNode* ref_node = navigate_to_node(reference_category_path, false);
    if (!ref_node) return false;
    
    std::string reference_name = reference_control_path.back();
    bool removed = false;
    auto cat_it = ref_node->category_references.find(reference_name);
    if (cat_it != ref_node->category_references.end()) {
        ref_node->category_references.erase(cat_it);
        removed = true;
    }
    auto alias_it = ref_node->control_aliases.find(reference_name);
    if (alias_it != ref_node->control_aliases.end()) {
        ref_node->control_aliases.erase(alias_it);
        removed = true;
    }
    return removed;
}