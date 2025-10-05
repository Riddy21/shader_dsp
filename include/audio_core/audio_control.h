#pragma once

#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <any>

// TODO: Add hierarchy for controls and names, so that controls can be searched and grouped
// TODO: Add ability to select from menu in controls

// Base control type: holds a setter and a value
class AudioControlBase {
public:
    virtual ~AudioControlBase() = default;
    virtual const std::string& name() const = 0;
    virtual void set_value(const std::any& value) = 0;
    virtual std::any get_value() const = 0;
};

template <typename T>
class AudioControl : public AudioControlBase {
public:
    using ValueType = T;

    // Constructor without an initial value; initializes to default and does not invoke setter
    AudioControl(const std::string& name, std::function<void(const ValueType&)> setter)
        : m_name(name), m_value(ValueType{}), m_setter(setter) {}

    // Constructor with an initial value; invokes setter on creation
    AudioControl(const std::string& name, const ValueType& initial_value, std::function<void(const ValueType&)> setter)
        : m_name(name), m_value(ValueType{}), m_setter(setter) {
        set(initial_value);
    }

    void set(const ValueType& value) {
        m_value = value;
        if (m_setter) m_setter(value);
    }

    const std::string& name() const override { return m_name; }

    void set_value(const std::any& value) override {
        if (value.type() == typeid(ValueType)) {
            set(std::any_cast<const ValueType&>(value));
        } else {
            throw std::bad_any_cast();
        }
    }

    // TODO: Change this to use template
    std::any get_value() const override { return m_value; }

    // TODO: Change this to use template
    const ValueType& value() const { return m_value; }

private:
    std::string m_name;
    ValueType m_value;
    std::function<void(const ValueType&)> m_setter;
};

// TODO: Should add a way to recall the control object as well
// Global registry for controls
class AudioControlRegistry {
public:
    static AudioControlRegistry& instance();

    // TODO: Add bool to return value
    void register_control(const std::string& name, AudioControlBase* control);
    void register_control(const std::string& category, const std::string& name, AudioControlBase* control);
    void register_control(const std::vector<std::string>& category_path, const std::string& name, AudioControlBase* control);

    // Typed deregistration that transfers ownership with correct type
    template <typename T>
    std::unique_ptr<AudioControl<T>> deregister_control(const std::string& name);
    template <typename T>
    std::unique_ptr<AudioControl<T>> deregister_control(const std::string& category, const std::string& name);
    template <typename T>
    std::unique_ptr<AudioControl<T>> deregister_control(const std::vector<std::string>& category_path, const std::string& name);

    template <typename T>
    bool set_control(const std::string& name, const T& value);
    template <typename T>
    bool set_control(const std::string& category, const std::string& name, const T& value);
    template <typename T>
    bool set_control(const std::vector<std::string>& category_path, const std::string& name, const T& value);

    template <typename T>
    AudioControl<T>* get_control(const std::string& name);
    template <typename T>
    AudioControl<T>* get_control(const std::string& category, const std::string& name);
    template <typename T>
    AudioControl<T>* get_control(const std::vector<std::string>& category_path, const std::string& name);

    std::vector<std::string> list_controls();
    std::vector<std::string> list_controls(const std::string& category);
    std::vector<std::string> list_controls(const std::vector<std::string>& category_path);
    std::vector<std::string> list_categories();
    std::vector<std::string> list_categories(const std::vector<std::string>& category_path);

private:
    struct CategoryNode {
        std::unordered_map<std::string, std::unique_ptr<CategoryNode>> children;
        std::unordered_map<std::string, std::unique_ptr<AudioControlBase>> controls;
    };
    CategoryNode m_root;
    std::mutex m_mutex;
    static const std::string& default_category();
    // Navigates to a node; creates missing nodes if create_if_missing is true.
    CategoryNode* navigate_to_node_unlocked(const std::vector<std::string>& category_path, bool create_if_missing);
    AudioControlRegistry();
    AudioControlRegistry(const AudioControlRegistry&) = delete;
    AudioControlRegistry& operator=(const AudioControlRegistry&) = delete;

    // Non-template helpers implemented in the .cpp so template wrappers can stay minimal
    bool set_control_any(const std::vector<std::string>& category_path, const std::string& name, const std::any& value);
    AudioControlBase* get_control_untyped(const std::vector<std::string>& category_path, const std::string& name);
    std::unique_ptr<AudioControlBase> deregister_control_if(
        const std::vector<std::string>& category_path,
        const std::string& name,
        const std::function<bool(AudioControlBase&)>& predicate);
};

#include "audio_control.tpp"
