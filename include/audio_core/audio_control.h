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
#include <typeindex>
#include <typeinfo>
#include <algorithm>
#include <stdexcept>

// TODO: Add hierarchy for controls and names, so that controls can be searched and grouped
// TODO: Add ability to select from menu in controls

// Forward declarations
class AudioControlRegistry;

// Base control type: holds a setter and a value
class AudioControlBase {
private:
    virtual void get_impl(const std::type_info& type, void * value) const = 0;
    virtual void set_impl(const std::type_info& type, const void * value) = 0;
    virtual void items_impl(const std::type_info& type, void * items) const = 0;

public:
    virtual ~AudioControlBase() = default;
    virtual const std::string& name() const = 0;
    virtual const std::type_index type() const = 0;

    template <typename T>
    void set(const T& value) {
        set_impl(typeid(T), &value);
    }

    template <typename T>
    T get() const {
        T value;
        get_impl(typeid(T), &value);
        return value;
    }

    template <typename T>
    std::vector<T> items() const {
        std::vector<T> items;
        items_impl(typeid(T), &items);
        return items;
    }
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

    const std::string& name() const override { return m_name; }
    const std::type_index type() const override { return typeid(ValueType); }

private:

    void set_impl(const std::type_info& type, const void * value) override {
        if (type == typeid(ValueType)) {
            const ValueType& typed_value = *static_cast<const ValueType*>(value);
            set(typed_value);
        } else {
            throw std::bad_cast();
        }
    }

    void get_impl(const std::type_info& type, void * value) const override {
        if (type == typeid(ValueType)) {
            *static_cast<ValueType*>(value) = m_value;
        } else {
            throw std::bad_cast();
        }
    }

    void items_impl(const std::type_info& type, void * items) const override {
        if (type == typeid(ValueType)) {
            *static_cast<std::vector<ValueType>*>(items) = {};
        } else {
            throw std::bad_cast();
        }
    }


    void set(const ValueType& value) {
        m_value = value;
        if (m_setter) m_setter(value);
    }

    std::string m_name;
    ValueType m_value;
    std::function<void(const ValueType&)> m_setter;
};

// Implement this after
template <typename T>
class AudioSelectionControl : public AudioControlBase {
public:
    using ValueType = T;
    AudioSelectionControl(const std::string& name, const std::vector<ValueType>& items, std::function<void(const ValueType&)> setter)
        : m_name(name), m_items(items), m_setter(setter) {}

    AudioSelectionControl(const std::string& name, const std::vector<ValueType>& items, const ValueType& initial_value, std::function<void(const ValueType&)> setter)
        : m_name(name), m_items(items), m_value(initial_value), m_setter(setter) {
            set(initial_value);
        }

    const std::string& name() const override { return m_name; }
    const std::type_index type() const override { return typeid(ValueType); }
    const std::vector<ValueType>& items() const { return m_items; }

private:

    void set_impl(const std::type_info& type, const void * value) override {
        if (type == typeid(ValueType)) {
            const ValueType& typed_value = *static_cast<const ValueType*>(value);
            // Ensure the new value exists in the allowed items
            if (std::find(m_items.begin(), m_items.end(), typed_value) == m_items.end()) {
                throw std::invalid_argument("Value is not one of the items");
            }
            set(typed_value);
        } else {
            throw std::bad_cast();
        }
    }

    void get_impl(const std::type_info& type, void * value) const override {

        if (type == typeid(ValueType)) {
            *static_cast<ValueType*>(value) = m_value;
        } else {
            throw std::bad_cast();
        }
    }

    void items_impl(const std::type_info& type, void * items) const override {
        if (type == typeid(ValueType)) {
            *static_cast<std::vector<ValueType>*>(items) = m_items;
        } else {
            throw std::bad_cast();
        }
    }

    void set(const ValueType& value) {
        m_value = value;
        if (m_setter) m_setter(value);
    }

    std::string m_name;
    std::vector<ValueType> m_items;
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
    
    std::shared_ptr<AudioControlBase> deregister_control(const std::vector<std::string>& control_path);

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
    // Keep retired controls alive when handles are updated
    std::vector<std::shared_ptr<AudioControlBase>> m_retired_controls;
    static const std::string& default_category();
    // Navigates to a node; creates missing nodes if create_if_missing is true.
    CategoryNode* navigate_to_node(const std::vector<std::string>& category_path, bool create_if_missing);
    AudioControlRegistry();
    AudioControlRegistry(const AudioControlRegistry&) = delete;
    AudioControlRegistry& operator=(const AudioControlRegistry&) = delete;

    // Non-template helpers implemented in the .cpp so template wrappers can stay minimal
    std::shared_ptr<AudioControlBase> deregister_control_if(
        const std::vector<std::string>& category_path,
        const std::string& name,
        const std::function<bool(AudioControlBase&)>& predicate);
};

 
