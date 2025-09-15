#include "tests/framework/test_main.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_output/audio_output.h"

// We'll create a mockable version of AudioOutput for testing
class MockableAudioOutput : public AudioOutput {
public:
    MockableAudioOutput(unsigned int buffer_size, unsigned int sample_rate, unsigned int num_channels) 
        : AudioOutput(buffer_size, sample_rate, num_channels) {}
    
    // Override the push method to use our mock system
    bool push(const float* buffer, bool blocking = false) override {
        // Check if a mock function is registered for this method
        if (Mock::exists("MockableAudioOutput_push")) {
            // Call the mock function
            return Mock::get<std::function<bool(const float*, bool)>>("MockableAudioOutput_push")(buffer, blocking);
        }
        
        // If no mock is registered, call the real implementation
        return AudioOutput::push(buffer, blocking);
    }

    // Make this method virtual to allow testing
    bool virtual is_ready() override {
        // Check if a mock function is registered for this method
        if (Mock::exists("MockableAudioOutput_is_ready")) {
            // Call the mock function
            return Mock::get<std::function<bool()>>("MockableAudioOutput_is_ready")();
        }
        
        // If no mock is registered, call the real implementation
        return AudioOutput::is_ready();
    }
};

TEST_CASE_WITH_MOCKS("AudioRenderer with mocked AudioOutput", "[mock]") {
    // Get the AudioRenderer instance
    AudioRenderer& renderer = AudioRenderer::get_instance();
    
    // Initialize the renderer
    REQUIRE(renderer.initialize(512, 44100, 2));
    
    // Create a mockable audio output
    auto audio_output = new MockableAudioOutput(512, 44100, 2);
    
    // Add the audio output to the renderer
    REQUIRE(renderer.add_render_output(audio_output));
    
    // Set up a mock for the push method
    int push_call_count = 0;
    Mock::when("MockableAudioOutput_push", [&push_call_count](const float* buffer, bool blocking) -> bool {
        push_call_count++;
        return true; // Always return success
    });
    
    // Set up a mock for the is_ready method
    Mock::when("MockableAudioOutput_is_ready", []() -> bool {
        return true; // Always return ready
    });
    
    // Access the private push_to_output_buffers method to test our mock
    float test_buffer[1024] = {0.1f, 0.2f, 0.3f}; // Sample buffer
    TestAccess<AudioRenderer>::call(renderer, &AudioRenderer::push_to_output_buffers, test_buffer);
    
    // Verify that our mock was called
    REQUIRE(push_call_count == 1);
    
    // Change the mock behavior
    Mock::when("MockableAudioOutput_is_ready", []() -> bool {
        return false; // Now return not ready
    });
    
    // Test that the mock changed behavior
    REQUIRE(audio_output->is_ready() == false);
}