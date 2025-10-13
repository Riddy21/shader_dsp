#pragma once

#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <any>
#include <optional>

// TODO: Add hierarchy for controls and names, so that controls can be searched and grouped
// TODO: Add ability to select from menu in controls

// Forward declarations
class AudioControlRegistry;

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

// Update ControlHandle to use double shared_ptr
class ControlHandle {
public:
    friend class AudioControlRegistry;
    ControlHandle() {}
    ControlHandle(std::shared_ptr<AudioControlBase> control)
        : m_control(control) {}

    AudioControlBase* get() const { return m_control.get(); }

    AudioControlBase* operator->() const { return get(); }
    AudioControlBase& operator*() const { return *get(); }
    explicit operator bool() const { return m_control != nullptr; }

private:
    std::shared_ptr<AudioControlBase> m_control;
};

// Global registry for controls
class AudioControlRegistry {
public:
    static AudioControlRegistry& instance();

    // Control operations using full paths (last element is control name, rest is category path)
    void register_control(const std::vector<std::string>& control_path, std::shared_ptr<AudioControlBase> control);
    
    // Typed deregistration that transfers ownership with correct type
    template <typename T>
    std::shared_ptr<AudioControl<T>> deregister_control(const std::vector<std::string>& control_path);

    bool deregister_control(const std::vector<std::string>& control_path);

    template <typename T>
    bool set_control(const std::vector<std::string>& control_path, const T& value);

    // Change return type of get_control
    ControlHandle & get_control(const std::vector<std::string>& control_path);

    std::vector<std::string> list_controls(const std::optional<std::vector<std::string>>& category_path = std::nullopt);
    std::vector<std::string> list_all_controls();

private:
    // Update CategoryNode to store shared_ptr<ControlHandle>
    struct CategoryNode {
        std::unordered_map<std::string, CategoryNode> children;
        std::unordered_map<std::string, ControlHandle> controls;
    };
    CategoryNode m_root;
    static const std::string& default_category();
    // Navigates to a node; creates missing nodes if create_if_missing is true.
    CategoryNode* navigate_to_node(const std::vector<std::string>& category_path, bool create_if_missing);
    AudioControlRegistry();
    AudioControlRegistry(const AudioControlRegistry&) = delete;
    AudioControlRegistry& operator=(const AudioControlRegistry&) = delete;

    // Non-template helpers implemented in the .cpp so template wrappers can stay minimal
    bool set_control_any(const std::vector<std::string>& control_path, const std::any& value);

    std::shared_ptr<AudioControlBase> deregister_control_if(
        const std::vector<std::string>& category_path,
        const std::string& name,
        const std::function<bool(AudioControlBase&)>& predicate);
};

#include "audio_control.tpp"
