#pragma once

#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

// Base control type: holds a setter and a value
class AudioControlBase {
public:
    virtual ~AudioControlBase() = default;
    virtual const std::string& name() const = 0;
};

template <typename T>
class AudioControl : public AudioControlBase {
public:
    using ValueType = T;

    AudioControl(const std::string& name, ValueType initial_value, std::function<void(const ValueType&)> setter)
        : m_name(name), m_value(initial_value), m_setter(setter) {}

    void set(const ValueType& value) {
        m_value = value;
        if (m_setter) m_setter(value);
    }

    const std::string& name() const override { return m_name; }
    const ValueType& value() const { return m_value; }

private:
    std::string m_name;
    ValueType m_value;
    std::function<void(const ValueType&)> m_setter;
};

// Global registry for controls
class AudioControlRegistry {
public:
    static AudioControlRegistry& instance() {
        static AudioControlRegistry inst;
        return inst;
    }

    template <typename T>
    void register_control(const std::string& name, AudioControl<T>* control) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_controls[name] = std::unique_ptr<AudioControlBase>(control);
    }

    template <typename T>
    bool set_control(const std::string& name, const T& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_controls.find(name);
        if (it != m_controls.end()) {
            auto control = dynamic_cast<AudioControl<T>*>(it->second.get());
            if (control) {
                control->set(value);
                return true;
            }
        }
        return false;
    }

    template <typename T>
    AudioControl<T>* get_control(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_controls.find(name);
        if (it != m_controls.end()) {
            return dynamic_cast<AudioControl<T>*>(it->second.get());
        }
        return nullptr;
    }

    std::vector<std::string> list_controls() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> names;
        for (const auto& kv : m_controls) names.push_back(kv.first);
        return names;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<AudioControlBase>> m_controls;
    std::mutex m_mutex;
    AudioControlRegistry() = default;
    AudioControlRegistry(const AudioControlRegistry&) = delete;
    AudioControlRegistry& operator=(const AudioControlRegistry&) = delete;
};
