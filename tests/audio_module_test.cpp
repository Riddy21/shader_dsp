#include "catch2/catch_all.hpp"
#include <memory>
#include <variant>
#include "audio_synthesizer/audio_module.h"

// Dummy module for testing control interface
class DummyModule : public AudioEffectModule {
public:
    DummyModule()
        : AudioEffectModule("dummy", 64, 44100, 2), gain(1.0f), enabled(false), mode(0) {
        m_float_controls.emplace_back("gain", gain, [this](const float& v) { gain = v; });
        m_bool_controls.emplace_back("enabled", enabled, [this](const bool& v) { enabled = v; });
        m_int_controls.emplace_back("mode", mode, [this](const int& v) { mode = v; });
    }

    bool set_control(const std::string& control_name, const std::variant<float, int, bool>& value) override {
        if (std::holds_alternative<float>(value)) {
            for (auto& c : m_float_controls) {
                if (c.name == control_name) {
                    c.setter(std::get<float>(value));
                    c.value = std::get<float>(value);
                    return true;
                }
            }
        } else if (std::holds_alternative<int>(value)) {
            for (auto& c : m_int_controls) {
                if (c.name == control_name) {
                    c.setter(std::get<int>(value));
                    c.value = std::get<int>(value);
                    return true;
                }
            }
        } else if (std::holds_alternative<bool>(value)) {
            for (auto& c : m_bool_controls) {
                if (c.name == control_name) {
                    c.setter(std::get<bool>(value));
                    c.value = std::get<bool>(value);
                    return true;
                }
            }
        }
        return false;
    }

    float gain;
    bool enabled;
    int mode;
private:
    std::vector<AudioModuleControl<float>> m_float_controls;
    std::vector<AudioModuleControl<int>> m_int_controls;
    std::vector<AudioModuleControl<bool>> m_bool_controls;
};

TEST_CASE("AudioModuleControl set and get controls", "[AudioModuleControl]") {
    // Create a dummy module
    DummyModule module;

}