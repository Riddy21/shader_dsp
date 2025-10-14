#include "catch2/catch_all.hpp"
#include "audio_core/audio_control.h"
#include <vector>
#include <string>
#include <optional>

TEST_CASE("AudioControlBase and AudioControl functionality", "[AudioControl]") {
    float test_value = 0.0f;
    auto* control = new AudioControl<float>("test", 1.0f, [&](const float& v) { test_value = v; });

    REQUIRE(control->name() == "test");
    REQUIRE(control->get<float>() == 1.0f);

    static_cast<AudioControlBase*>(control)->set<float>(2.5f);
    REQUIRE(control->get<float>() == 2.5f);
    REQUIRE(test_value == 2.5f);

    // Test polymorphic access via base pointer
    AudioControlBase* base_ptr = control;
    REQUIRE(base_ptr->name() == "test");

    // Register control in the registry using path-based API
    AudioControlRegistry::instance().register_control({"control", "test"}, std::shared_ptr<AudioControlBase>(control));

    // Retrieve control from registry and check pointer equality
    auto & reg_control = AudioControlRegistry::instance().get_control({"control", "test"});
    REQUIRE(reg_control.get() == control);

    // Set value via registry and check propagation
    auto & handle0 = AudioControlRegistry::instance().get_control({"control", "test"});
    REQUIRE(static_cast<bool>(handle0));
    handle0->set<float>(7.5f);
    REQUIRE(control->get<float>() == 7.5f);
    REQUIRE(test_value == 7.5f);

    // List controls and check presence
    auto controls = AudioControlRegistry::instance().list_all_controls();
    bool found = false;
    for (const auto& name : controls) {
        if (name == "control/test") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("AudioControl constructor without initial value does not invoke setter", "[AudioControl]") {
    int call_count = 0;
    auto* control = new AudioControl<int>("no_initial", [&](const int&){ call_count++; });
    // Constructor should not call setter
    REQUIRE(call_count == 0);
    // Value should be default-initialized (0 for int)
    REQUIRE(control->get<int>() == 0);
    // Now setting should invoke setter
    static_cast<AudioControlBase*>(control)->set<int>(5);
    REQUIRE(call_count == 1);
    REQUIRE(control->get<int>() == 5);
}

TEST_CASE("AudioControl with different types", "[AudioControl]") {
    // Test int control
    int int_value = 0;
    auto* int_control = new AudioControl<int>("int_control", 42, [&](const int& v) { int_value = v; });
    
    REQUIRE(int_control->get<int>() == 42);
    static_cast<AudioControlBase*>(int_control)->set<int>(100);
    REQUIRE(int_control->get<int>() == 100);
    REQUIRE(int_value == 100);

    // Test vector control
    std::vector<float> vector_value;
    auto* vector_control = new AudioControl<std::vector<float>>(
        "vector_control", 
        {1.0f, 2.0f}, 
        [&](const std::vector<float>& v) { vector_value = v; }
    );
    
    REQUIRE(vector_control->get<std::vector<float>>() == std::vector<float>{1.0f, 2.0f});
    static_cast<AudioControlBase*>(vector_control)->set<std::vector<float>>({3.0f, 4.0f, 5.0f});
    REQUIRE(vector_control->get<std::vector<float>>() == std::vector<float>{3.0f, 4.0f, 5.0f});
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

    // Register all controls using path-based API
    AudioControlRegistry::instance().register_control({"test_int"}, std::shared_ptr<AudioControlBase>(int_ctrl));
    AudioControlRegistry::instance().register_control({"test_float"}, std::shared_ptr<AudioControlBase>(float_ctrl));
    AudioControlRegistry::instance().register_control({"test_string"}, std::shared_ptr<AudioControlBase>(string_ctrl));
    AudioControlRegistry::instance().register_control({"test_vector"}, std::shared_ptr<AudioControlBase>(vector_ctrl));

    // Test retrieval
    auto & retrieved_int = AudioControlRegistry::instance().get_control({"test_int"});
    auto & retrieved_float = AudioControlRegistry::instance().get_control({"test_float"});
    auto & retrieved_string = AudioControlRegistry::instance().get_control({"test_string"});
    auto & retrieved_vector = AudioControlRegistry::instance().get_control({"test_vector"});

    REQUIRE(retrieved_int.get() == int_ctrl);
    REQUIRE(retrieved_float.get() == float_ctrl);
    REQUIRE(retrieved_string.get() == string_ctrl);
    REQUIRE(retrieved_vector.get() == vector_ctrl);

    // Test setting values through registry via handle
    AudioControlRegistry::instance().get_control({"test_int"})->set<int>(20);
    AudioControlRegistry::instance().get_control({"test_float"})->set<float>(2.71f);
    AudioControlRegistry::instance().get_control({"test_string"})->set<std::string>("world");
    AudioControlRegistry::instance().get_control({"test_vector"})->set<std::vector<float>>({3.0f, 4.0f});

    REQUIRE(int_val == 20);
    REQUIRE(float_val == 2.71f);
    REQUIRE(string_val == "world");
    REQUIRE(vector_val == std::vector<float>{3.0f, 4.0f});

    // Test listing controls
    auto controls = AudioControlRegistry::instance().list_controls();
    REQUIRE(controls.size() >= 4);
    
    std::vector<std::string> expected_names = {"test_int", "test_float", "test_string", "test_vector"};
    for (const auto& expected : expected_names) {
        bool found = false;
        for (const auto& name : controls) {
            if (name == expected) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("AudioControl error handling", "[AudioControl]") {
    // Test non-existent control
    auto & non_existent = AudioControlRegistry::instance().get_control({"non_existent"});
    REQUIRE(static_cast<bool>(non_existent) == false);

    auto & missing_handle = AudioControlRegistry::instance().get_control({"non_existent"});
    REQUIRE(static_cast<bool>(missing_handle) == false);

    // Test std::any casting with wrong type
    auto* control = new AudioControl<float>("type_test", 1.0f, [](const float&) {});
    
    // This should work
    static_cast<AudioControlBase*>(control)->set<float>(2.0f);
    REQUIRE(control->get<float>() == 2.0f);

    // This should throw
    REQUIRE_THROWS_AS(static_cast<AudioControlBase*>(control)->set<int>(42), std::bad_cast);
}

TEST_CASE("AudioControlRegistry supports categories with same control names", "[AudioControl]") {
    // Arrange two categories with identical control names
    float piano_freq = 0.0f;
    float synth_freq = 0.0f;

    auto* piano_play = new AudioControl<float>("play_note", 0.0f, [&](const float& v) { piano_freq = v; });
    auto* synth_play = new AudioControl<float>("play_note", 0.0f, [&](const float& v) { synth_freq = v; });

    // Register under different categories using path-based API
    AudioControlRegistry::instance().register_control({"piano", "play_note"}, std::shared_ptr<AudioControlBase>(piano_play));
    AudioControlRegistry::instance().register_control({"synth", "play_note"}, std::shared_ptr<AudioControlBase>(synth_play));

    // Verify retrieval by path
    auto & piano_get = AudioControlRegistry::instance().get_control({"piano", "play_note"});
    auto & synth_get = AudioControlRegistry::instance().get_control({"synth", "play_note"});
    REQUIRE(piano_get.get() == piano_play);
    REQUIRE(synth_get.get() == synth_play);

    // Verify setting by path updates the correct target only
    AudioControlRegistry::instance().get_control({"piano", "play_note"})->set<float>(440.0f);
    REQUIRE(piano_freq == 440.0f);
    REQUIRE(synth_freq == 0.0f);

    AudioControlRegistry::instance().get_control({"synth", "play_note"})->set<float>(220.0f);
    REQUIRE(piano_freq == 440.0f);
    REQUIRE(synth_freq == 220.0f);

    // Verify listings
    auto categories = AudioControlRegistry::instance().list_controls();
    bool piano_found = false, synth_found = false;
    for (const auto& name : categories) {
        if (name == "piano") piano_found = true;
        if (name == "synth") synth_found = true;
    }
    REQUIRE(piano_found);
    REQUIRE(synth_found);

    auto piano_controls = AudioControlRegistry::instance().list_controls(std::optional<std::vector<std::string>>{{"piano"}});
    auto synth_controls = AudioControlRegistry::instance().list_controls(std::optional<std::vector<std::string>>{{"synth"}});
    
    bool piano_play_found = false, synth_play_found = false;
    for (const auto& name : piano_controls) {
        if (name == "play_note") piano_play_found = true;
    }
    for (const auto& name : synth_controls) {
        if (name == "play_note") synth_play_found = true;
    }
    REQUIRE(piano_play_found);
    REQUIRE(synth_play_found);
}

TEST_CASE("AudioControlRegistry supports hierarchical categories", "[AudioControl]") {
    float lead_freq = 0.0f;
    float pad_freq = 0.0f;

    auto* lead_play = new AudioControl<float>("play_note", 0.0f, [&](const float& v) { lead_freq = v; });
    auto* pad_play = new AudioControl<float>("play_note", 0.0f, [&](const float& v) { pad_freq = v; });

    // Register under nested category paths using path-based API
    AudioControlRegistry::instance().register_control({"synth", "lead", "play_note"}, std::shared_ptr<AudioControlBase>(lead_play));
    AudioControlRegistry::instance().register_control({"synth", "pad", "play_note"}, std::shared_ptr<AudioControlBase>(pad_play));

    // Retrieve by path
    auto & lead_get = AudioControlRegistry::instance().get_control({"synth", "lead", "play_note"});
    auto & pad_get  = AudioControlRegistry::instance().get_control({"synth", "pad", "play_note"});
    REQUIRE(lead_get.get() == lead_play);
    REQUIRE(pad_get.get()  == pad_play);

    // Set by path
    AudioControlRegistry::instance().get_control({"synth", "lead", "play_note"})->set<float>(880.0f);
    REQUIRE(lead_freq == 880.0f);
    REQUIRE(pad_freq == 0.0f);

    AudioControlRegistry::instance().get_control({"synth", "pad", "play_note"})->set<float>(110.0f);
    REQUIRE(lead_freq == 880.0f);
    REQUIRE(pad_freq == 110.0f);

    // List subcategories under synth
    auto subcats = AudioControlRegistry::instance().list_controls(std::optional<std::vector<std::string>>{{"synth"}});
    bool lead_found = false, pad_found = false;
    for (const auto& name : subcats) {
        if (name == "lead") lead_found = true;
        if (name == "pad") pad_found = true;
    }
    REQUIRE(lead_found);
    REQUIRE(pad_found);

    // List controls at a node
    auto lead_controls = AudioControlRegistry::instance().list_controls(std::optional<std::vector<std::string>>{{"synth", "lead"}});
    bool play_note_found = false;
    for (const auto& name : lead_controls) {
        if (name == "play_note") {
            play_note_found = true;
            break;
        }
    }
    REQUIRE(play_note_found);
}

TEST_CASE("AudioControlRegistry can deregister and transfer ownership", "[AudioControl]") {
    int call_count = 0;
    auto* ctrl = new AudioControl<int>("param", 1, [&](const int&){ call_count++; });

    // Register under nested path using path-based API
    AudioControlRegistry::instance().register_control({"fx", "reverb", "param"}, std::shared_ptr<AudioControlBase>(ctrl));

    // Ensure it exists
    auto & found = AudioControlRegistry::instance().get_control({"fx", "reverb", "param"});
    REQUIRE(found.get() == ctrl);

    // Deregister transfers ownership (typed) using path-based API
    auto ptr = AudioControlRegistry::instance().deregister_control({"fx", "reverb", "param"});
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr->get<int>() == 1);

    // No longer retrievable
    auto & not_found = AudioControlRegistry::instance().get_control({"fx", "reverb", "param"});
    REQUIRE(static_cast<bool>(not_found) == false);

    // Manipulate after transfer (outside registry) still works
    ptr->set(42);
    REQUIRE(call_count == 2);
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
    static_cast<AudioControlBase*>(value_control)->set<int>(42);
    REQUIRE(test_obj.value == 42);

    static_cast<AudioControlBase*>(name_control)->set<std::string>("test_name");
    REQUIRE(test_obj.name == "test_name");

    // Test through registry using path-based API
    AudioControlRegistry::instance().register_control({"member_value"}, std::shared_ptr<AudioControlBase>(value_control));
    AudioControlRegistry::instance().register_control({"member_name"}, std::shared_ptr<AudioControlBase>(name_control));

    AudioControlRegistry::instance().get_control({"member_value"})->set<int>(100);
    AudioControlRegistry::instance().get_control({"member_name"})->set<std::string>("registry_name");


    REQUIRE(test_obj.value == 100);
    REQUIRE(test_obj.name == "registry_name");
}

TEST_CASE("AudioControlRegistry get_control linkage works", "[AudioControl]") {
    auto & registry = AudioControlRegistry::instance();

    auto* ctrl1 = new AudioControl<int>("ctrl1", 0, [](const int&){});
    auto* ctrl2 = new AudioControl<int>("ctrl2", 0, [](const int&){});

    registry.register_control({"cat1", "ctrl1"}, std::shared_ptr<AudioControlBase>(ctrl1));

    auto & handle = registry.get_control({"cat1", "ctrl1"});

    handle->set<int>(10);

    REQUIRE(ctrl1->get<int>() == 10);

    registry.register_control({"cat1", "ctrl1"}, std::shared_ptr<AudioControlBase>(ctrl2));

    handle->set<int>(20);

    REQUIRE(ctrl2->get<int>() == 20);
    REQUIRE(ctrl1->get<int>() == 10);
    REQUIRE(handle->get<int>() == 20);
}

TEST_CASE("AudioSelectionControl functionality", "[AudioSelectionControl]") {
    std::vector<std::string> items = {"item1", "item2", "item3"};
    std::string selected_item = "item2";

    auto* control = new AudioSelectionControl<std::string>("selection", items, selected_item, [&](const std::string& v) { selected_item = v; });

    REQUIRE(control->name() == "selection");
    REQUIRE(control->items() == items);
    REQUIRE(control->get<std::string>() == selected_item);

    // Register in the registry
    AudioControlRegistry::instance().register_control({"selection"}, std::shared_ptr<AudioControlBase>(control));

    // Retrieve from the registry
    auto & retrieved = AudioControlRegistry::instance().get_control({"selection"});

    // Check pointer equality
    REQUIRE(retrieved.get() == control);

    // Get values from the registry
    // TODO: Find way to do this easily
}