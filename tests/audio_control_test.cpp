#include "catch2/catch_all.hpp"
#include "audio_core/audio_control.h"
#include <vector>
#include <string>
#include <optional>

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

    // Register control in the registry using path-based API
    AudioControlRegistry::instance().register_control({"test_control"}, control);

    // Retrieve control from registry and check pointer equality
    auto* reg_control = AudioControlRegistry::instance().get_control<float>({"test_control"});
    REQUIRE(reg_control == control);

    // Set value via registry and check propagation
    bool set_result = AudioControlRegistry::instance().set_control<float>({"test_control"}, 7.5f);
    REQUIRE(set_result);
    REQUIRE(control->value() == 7.5f);
    REQUIRE(test_value == 7.5f);

    // List controls and check presence
    auto controls = AudioControlRegistry::instance().list_controls();
    bool found = false;
    for (const auto& name : controls) {
        if (name == "test_control") {
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
    REQUIRE(control->value() == 0);
    // Now setting should invoke setter
    control->set(5);
    REQUIRE(call_count == 1);
    REQUIRE(control->value() == 5);
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

    // Register all controls using path-based API
    AudioControlRegistry::instance().register_control({"test_int"}, int_ctrl);
    AudioControlRegistry::instance().register_control({"test_float"}, float_ctrl);
    AudioControlRegistry::instance().register_control({"test_string"}, string_ctrl);
    AudioControlRegistry::instance().register_control({"test_vector"}, vector_ctrl);

    // Test retrieval
    auto* retrieved_int = AudioControlRegistry::instance().get_control<int>({"test_int"});
    auto* retrieved_float = AudioControlRegistry::instance().get_control<float>({"test_float"});
    auto* retrieved_string = AudioControlRegistry::instance().get_control<std::string>({"test_string"});
    auto* retrieved_vector = AudioControlRegistry::instance().get_control<std::vector<float>>({"test_vector"});

    REQUIRE(retrieved_int == int_ctrl);
    REQUIRE(retrieved_float == float_ctrl);
    REQUIRE(retrieved_string == string_ctrl);
    REQUIRE(retrieved_vector == vector_ctrl);

    // Test setting values through registry
    REQUIRE(AudioControlRegistry::instance().set_control<int>({"test_int"}, 20));
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"test_float"}, 2.71f));
    REQUIRE(AudioControlRegistry::instance().set_control<std::string>({"test_string"}, "world"));
    REQUIRE(AudioControlRegistry::instance().set_control<std::vector<float>>({"test_vector"}, {3.0f, 4.0f}));

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
    auto* non_existent = AudioControlRegistry::instance().get_control<float>({"non_existent"});
    REQUIRE(non_existent == nullptr);

    bool set_result = AudioControlRegistry::instance().set_control<float>({"non_existent"}, 1.0f);
    REQUIRE(set_result == false);

    // Test std::any casting with wrong type
    auto* control = new AudioControl<float>("type_test", 1.0f, [](const float&) {});
    
    // This should work
    control->set_value(std::any(2.0f));
    REQUIRE(control->value() == 2.0f);

    // This should throw
    REQUIRE_THROWS_AS(control->set_value(std::any(42)), std::bad_any_cast);
}

TEST_CASE("AudioControlRegistry supports categories with same control names", "[AudioControl]") {
    // Arrange two categories with identical control names
    float piano_freq = 0.0f;
    float synth_freq = 0.0f;

    auto* piano_play = new AudioControl<float>("play_note", 0.0f, [&](const float& v) { piano_freq = v; });
    auto* synth_play = new AudioControl<float>("play_note", 0.0f, [&](const float& v) { synth_freq = v; });

    // Register under different categories using path-based API
    AudioControlRegistry::instance().register_control({"piano", "play_note"}, piano_play);
    AudioControlRegistry::instance().register_control({"synth", "play_note"}, synth_play);

    // Verify retrieval by path
    auto* piano_get = AudioControlRegistry::instance().get_control<float>({"piano", "play_note"});
    auto* synth_get = AudioControlRegistry::instance().get_control<float>({"synth", "play_note"});
    REQUIRE(piano_get == piano_play);
    REQUIRE(synth_get == synth_play);

    // Verify setting by path updates the correct target only
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"piano", "play_note"}, 440.0f));
    REQUIRE(piano_freq == 440.0f);
    REQUIRE(synth_freq == 0.0f);

    REQUIRE(AudioControlRegistry::instance().set_control<float>({"synth", "play_note"}, 220.0f));
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
    AudioControlRegistry::instance().register_control({"synth", "lead", "play_note"}, lead_play);
    AudioControlRegistry::instance().register_control({"synth", "pad", "play_note"}, pad_play);

    // Retrieve by path
    auto* lead_get = AudioControlRegistry::instance().get_control<float>({"synth", "lead", "play_note"});
    auto* pad_get  = AudioControlRegistry::instance().get_control<float>({"synth", "pad", "play_note"});
    REQUIRE(lead_get == lead_play);
    REQUIRE(pad_get  == pad_play);

    // Set by path
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"synth", "lead", "play_note"}, 880.0f));
    REQUIRE(lead_freq == 880.0f);
    REQUIRE(pad_freq == 0.0f);

    REQUIRE(AudioControlRegistry::instance().set_control<float>({"synth", "pad", "play_note"}, 110.0f));
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
    AudioControlRegistry::instance().register_control({"fx", "reverb", "param"}, ctrl);

    // Ensure it exists
    auto* found = AudioControlRegistry::instance().get_control<int>({"fx", "reverb", "param"});
    REQUIRE(found == ctrl);

    // Deregister transfers ownership (typed) using path-based API
    auto ptr = AudioControlRegistry::instance().deregister_control<int>({"fx", "reverb", "param"});
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr.get() == ctrl);

    // No longer retrievable
    auto* not_found = AudioControlRegistry::instance().get_control<int>({"fx", "reverb", "param"});
    REQUIRE(not_found == nullptr);

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
    value_control->set(42);
    REQUIRE(test_obj.value == 42);

    name_control->set("test_name");
    REQUIRE(test_obj.name == "test_name");

    // Test through registry using path-based API
    AudioControlRegistry::instance().register_control({"member_value"}, value_control);
    AudioControlRegistry::instance().register_control({"member_name"}, name_control);

    AudioControlRegistry::instance().set_control<int>({"member_value"}, 100);
    AudioControlRegistry::instance().set_control<std::string>({"member_name"}, "registry_name");

    REQUIRE(test_obj.value == 100);
    REQUIRE(test_obj.name == "registry_name");
}

TEST_CASE("AudioControlRegistry control references", "[AudioControl]") {
    float target_value = 0.0f;
    auto* target_control = new AudioControl<float>("target", 1.0f, [&](const float& v) { target_value = v; });
    
    // Register the target control
    AudioControlRegistry::instance().register_control({"category", "target"}, target_control);
    
    // Create a reference to the target control
    AudioControlRegistry::instance().link_control({"reference"}, {"category", "target"});
    
    // Test that the reference works
    auto* ref_control = AudioControlRegistry::instance().get_control<float>({"reference"});
    REQUIRE(ref_control == target_control);
    
    // Test setting through reference
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"reference"}, 5.0f));
    REQUIRE(target_value == 5.0f);
    REQUIRE(target_control->value() == 5.0f);
    
    // Test unlinking
    REQUIRE(AudioControlRegistry::instance().unlink_control({"reference"}));
    auto* unlinked_ref = AudioControlRegistry::instance().get_control<float>({"reference"});
    REQUIRE(unlinked_ref == nullptr);
}

TEST_CASE("AudioControlRegistry supports category alias links and mid-path control aliasing", "[AudioControl]") {
    // Arrange nested categories with controls
    float lead_val = 0.0f;
    float pad_val = 0.0f;
    auto* lead_ctrl = new AudioControl<float>("cutoff", 0.0f, [&](const float& v) { lead_val = v; });
    auto* pad_ctrl  = new AudioControl<float>("cutoff", 0.0f, [&](const float& v) { pad_val = v; });

    AudioControlRegistry::instance().register_control({"synth", "lead", "cutoff"}, lead_ctrl);
    AudioControlRegistry::instance().register_control({"synth", "pad",  "cutoff"}, pad_ctrl);

    // Create a category alias at root pointing to synth/lead
    // Accessing {"lead_alias", "cutoff"} should resolve to {"synth","lead","cutoff"}
    REQUIRE_NOTHROW(AudioControlRegistry::instance().link_control({"lead_alias"}, {"synth", "lead"}));

    // Verify retrieval through category alias
    auto* alias_ctrl = AudioControlRegistry::instance().get_control<float>({"lead_alias", "cutoff"});
    REQUIRE(alias_ctrl == lead_ctrl);

    // Set via alias path
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"lead_alias", "cutoff"}, 123.0f));
    REQUIRE(lead_val == 123.0f);
    REQUIRE(pad_val == 0.0f);

    // Create a control alias at root that points directly to a specific control under synth/pad
    REQUIRE_NOTHROW(AudioControlRegistry::instance().link_control({"pad_cutoff_link"}, {"synth", "pad", "cutoff"}));

    // Retrieving by the alias name as a control should return the pad control
    auto* pad_alias_ctrl = AudioControlRegistry::instance().get_control<float>({"pad_cutoff_link"});
    REQUIRE(pad_alias_ctrl == pad_ctrl);

    // Setting through the alias should update pad only
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"pad_cutoff_link"}, 77.0f));
    REQUIRE(pad_val == 77.0f);
    REQUIRE(lead_val == 123.0f);

    // Unlink control alias
    REQUIRE(AudioControlRegistry::instance().unlink_control({"pad_cutoff_link"}));
    auto* pad_alias_after_unlink = AudioControlRegistry::instance().get_control<float>({"pad_cutoff_link"});
    REQUIRE(pad_alias_after_unlink == nullptr);

    // Unlink category alias
    REQUIRE(AudioControlRegistry::instance().unlink_control({"lead_alias"}));
    auto* lead_alias_after_unlink = AudioControlRegistry::instance().get_control<float>({"lead_alias", "cutoff"});
    REQUIRE(lead_alias_after_unlink == nullptr);
}

TEST_CASE("AudioControlRegistry list_all_controls traverses hierarchy without following links", "[AudioControl]") {
    auto* ctrl1 = new AudioControl<int>("ctrl1", 0, [](const int&){});
    auto* ctrl2 = new AudioControl<int>("ctrl2", 0, [](const int&){});
    auto* ctrl3 = new AudioControl<int>("ctrl3", 0, [](const int&){});

    AudioControlRegistry::instance().register_control({"cat1", "ctrl1"}, ctrl1);
    AudioControlRegistry::instance().register_control({"cat1", "subcat", "ctrl2"}, ctrl2);
    AudioControlRegistry::instance().register_control({"cat2", "ctrl3"}, ctrl3);

    AudioControlRegistry::instance().link_control({"alias"}, {"cat1"});
    AudioControlRegistry::instance().link_control({"alias_ctrl"}, {"cat1", "ctrl1"});

    auto all = AudioControlRegistry::instance().list_all_controls();

    std::vector<std::string> expected = {"cat1/ctrl1", "cat1/subcat/ctrl2", "cat2/ctrl3"};
    REQUIRE(all.size() == 3);
    std::sort(all.begin(), all.end());
    std::sort(expected.begin(), expected.end());
    REQUIRE(all == expected);

    auto ptr1 = AudioControlRegistry::instance().deregister_control<int>({"cat1", "ctrl1"});
    auto ptr2 = AudioControlRegistry::instance().deregister_control<int>({"cat1", "subcat", "ctrl2"});
    auto ptr3 = AudioControlRegistry::instance().deregister_control<int>({"cat2", "ctrl3"});

    AudioControlRegistry::instance().unlink_control({"alias"});
    AudioControlRegistry::instance().unlink_control({"alias_ctrl"});
}