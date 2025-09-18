#include "catch2/catch_all.hpp"
#include "audio_core/audio_control.h"
#include <vector>
#include <string>

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
    AudioControlRegistry::instance().register_control("test_control", control);

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

TEST_CASE("AudioControl with different types", "[AudioControl]") {
    // Test int control
    int int_value = 0;
    auto* int_control = new AudioControl<int>("int_control", 42, [&](const int& v) { int_value = v; });
    
    REQUIRE(int_control->value() == 42);
    int_control->set(100);
    REQUIRE(int_control->value() == 100);
    REQUIRE(int_value == 100);

    // Test vector control
    std::vector<float> vector_value;
    auto* vector_control = new AudioControl<std::vector<float>>(
        "vector_control", 
        {1.0f, 2.0f}, 
        [&](const std::vector<float>& v) { vector_value = v; }
    );
    
    REQUIRE(vector_control->value() == std::vector<float>{1.0f, 2.0f});
    vector_control->set({3.0f, 4.0f, 5.0f});
    REQUIRE(vector_control->value() == std::vector<float>{3.0f, 4.0f, 5.0f});
    REQUIRE(vector_value == std::vector<float>{3.0f, 4.0f, 5.0f});
}

TEST_CASE("AudioControlRegistry with multiple controls", "[AudioControl]") {
    // Create multiple controls of different types
    int int_val = 0;
    float float_val = 0.0f;
    std::string string_val;
    std::vector<float> vector_val;
    
    auto* int_ctrl = new AudioControl<int>("test_int", 10, [&](const int& v) { int_val = v; });
    auto* float_ctrl = new AudioControl<float>("test_float", 3.14f, [&](const float& v) { float_val = v; });
    auto* string_ctrl = new AudioControl<std::string>("test_string", "hello", [&](const std::string& v) { string_val = v; });
    auto* vector_ctrl = new AudioControl<std::vector<float>>("test_vector", {1.0f, 2.0f}, [&](const std::vector<float>& v) { vector_val = v; });

    // Register all controls
    AudioControlRegistry::instance().register_control("test_int", int_ctrl);
    AudioControlRegistry::instance().register_control("test_float", float_ctrl);
    AudioControlRegistry::instance().register_control("test_string", string_ctrl);
    AudioControlRegistry::instance().register_control("test_vector", vector_ctrl);

    // Test retrieval
    auto* retrieved_int = AudioControlRegistry::instance().get_control<int>("test_int");
    auto* retrieved_float = AudioControlRegistry::instance().get_control<float>("test_float");
    auto* retrieved_string = AudioControlRegistry::instance().get_control<std::string>("test_string");
    auto* retrieved_vector = AudioControlRegistry::instance().get_control<std::vector<float>>("test_vector");

    REQUIRE(retrieved_int == int_ctrl);
    REQUIRE(retrieved_float == float_ctrl);
    REQUIRE(retrieved_string == string_ctrl);
    REQUIRE(retrieved_vector == vector_ctrl);

    // Test setting values through registry
    REQUIRE(AudioControlRegistry::instance().set_control<int>("test_int", 20));
    REQUIRE(AudioControlRegistry::instance().set_control<float>("test_float", 2.71f));
    REQUIRE(AudioControlRegistry::instance().set_control<std::string>("test_string", "world"));
    REQUIRE(AudioControlRegistry::instance().set_control<std::vector<float>>("test_vector", {3.0f, 4.0f}));

    REQUIRE(int_val == 20);
    REQUIRE(float_val == 2.71f);
    REQUIRE(string_val == "world");
    REQUIRE(vector_val == std::vector<float>{3.0f, 4.0f});

    // Test listing controls
    auto controls = AudioControlRegistry::instance().list_controls();
    REQUIRE(controls.size() >= 4);
    REQUIRE(std::find(controls.begin(), controls.end(), "test_int") != controls.end());
    REQUIRE(std::find(controls.begin(), controls.end(), "test_float") != controls.end());
    REQUIRE(std::find(controls.begin(), controls.end(), "test_string") != controls.end());
    REQUIRE(std::find(controls.begin(), controls.end(), "test_vector") != controls.end());
}

TEST_CASE("AudioControl error handling", "[AudioControl]") {
    // Test non-existent control
    auto* non_existent = AudioControlRegistry::instance().get_control<float>("non_existent");
    REQUIRE(non_existent == nullptr);

    bool set_result = AudioControlRegistry::instance().set_control<float>("non_existent", 1.0f);
    REQUIRE(set_result == false);

    // Test std::any casting with wrong type
    auto* control = new AudioControl<float>("type_test", 1.0f, [](const float&) {});
    
    // This should work
    control->set_value(std::any(2.0f));
    REQUIRE(control->value() == 2.0f);

    // This should throw
    REQUIRE_THROWS_AS(control->set_value(std::any(42)), std::bad_any_cast);
}

TEST_CASE("AudioControl with member function simulation", "[AudioControl]") {
    // Simulate a class with member functions
    struct TestClass {
        int value = 0;
        std::string name;
        
        void set_value(int v) { value = v; }
        void set_name(const std::string& n) { name = n; }
    };

    TestClass test_obj;
    
    // Create controls that call member functions
    auto* value_control = new AudioControl<int>(
        "member_value", 
        0, 
        [&test_obj](const int& v) { test_obj.set_value(v); }
    );
    
    auto* name_control = new AudioControl<std::string>(
        "member_name", 
        "", 
        [&test_obj](const std::string& n) { test_obj.set_name(n); }
    );

    // Test the controls
    value_control->set(42);
    REQUIRE(test_obj.value == 42);

    name_control->set("test_name");
    REQUIRE(test_obj.name == "test_name");

    // Test through registry
    AudioControlRegistry::instance().register_control("member_value", value_control);
    AudioControlRegistry::instance().register_control("member_name", name_control);

    AudioControlRegistry::instance().set_control<int>("member_value", 100);
    AudioControlRegistry::instance().set_control<std::string>("member_name", "registry_name");

    REQUIRE(test_obj.value == 100);
    REQUIRE(test_obj.name == "registry_name");
}

