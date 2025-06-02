#include "catch2/catch_all.hpp"
#include "audio_core/audio_control.h"

TEST_CASE("AudioControlBase and AudioControl functionality", "[AudioControl]") {
    float test_value = 0.0f;
    auto* control = new AudioControl<float>("test_control", 1.0f, [&](const float& v) { test_value = v; });

    REQUIRE(control->name() == "test_control");
    REQUIRE(control->value() == 1.0f);

    control->set(2.5f);
    REQUIRE(control->value() == 2.5f);
    REQUIRE(test_value == 2.5f);

    // Test polymorphic access via base pointer
    AudioControlBase* base_ptr = control;
    REQUIRE(base_ptr->name() == "test_control");

    // Register control in the registry
    AudioControlRegistry::instance().register_control<float>("test_control", control);

    // Retrieve control from registry and check pointer equality
    auto* reg_control = AudioControlRegistry::instance().get_control<float>("test_control");
    REQUIRE(reg_control == control);

    // Set value via registry and check propagation
    bool set_result = AudioControlRegistry::instance().set_control<float>("test_control", 7.5f);
    REQUIRE(set_result);
    REQUIRE(control->value() == 7.5f);
    REQUIRE(test_value == 7.5f);

    // List controls and check presence
    auto controls = AudioControlRegistry::instance().list_controls();
    REQUIRE(std::find(controls.begin(), controls.end(), "test_control") != controls.end());
}

