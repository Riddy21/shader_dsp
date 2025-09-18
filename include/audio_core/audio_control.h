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

    AudioControl(const std::string& name, ValueType initial_value, std::function<void(const ValueType&)> setter)
        : m_name(name), m_value(initial_value), m_setter(setter) {}

    void set(const ValueType& value) {
        m_value = value;
        if (m_setter) m_setter(value);
    }

    const std::string& name() const override { return m_name; }

    void set_value(const std::any& value) override {
        if (value.type() == typeid(ValueType)) {
            set(std::any_cast<ValueType>(value));
        } else {
            throw std::bad_any_cast();
        }
    }

    std::any get_value() const override {
        return m_value;
    }

    const ValueType& value() const { return m_value; }

private:
    std::string m_name;
    ValueType m_value;
    std::function<void(const ValueType&)> m_setter;
};

// Global registry for controls
class AudioControlRegistry {
public:
    static AudioControlRegistry& instance();

    void register_control(const std::string& name, AudioControlBase* control);

    template <typename T>
    bool set_control(const std::string& name, const T& value);

    template <typename T>
    AudioControl<T>* get_control(const std::string& name);

    std::vector<std::string> list_controls();

private:
    std::unordered_map<std::string, std::unique_ptr<AudioControlBase>> m_controls;
    std::mutex m_mutex;
    AudioControlRegistry();
    AudioControlRegistry(const AudioControlRegistry&) = delete;
    AudioControlRegistry& operator=(const AudioControlRegistry&) = delete;
};

#include "audio_control.tpp"
