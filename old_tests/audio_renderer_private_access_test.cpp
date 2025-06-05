#include "tests/framework/test_main.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"

// Test case demonstrating access to private members
TEST_CASE("AudioRenderer private member access") {
    // Get the AudioRenderer instance
    AudioRenderer& renderer = AudioRenderer::get_instance();
    
    // Initialize the renderer (a public method)
    REQUIRE(renderer.initialize(512, 44100, 2));
    
    // Access private member variables using our TestAccess template
    SECTION("Access to private member variables") {
        // Get the private buffer_size variable
        unsigned int& buffer_size = TestAccess<AudioRenderer>::get(renderer, &AudioRenderer::m_buffer_size);
        REQUIRE(buffer_size == 512);
        
        // Get and modify the private frame_count variable
        unsigned int& frame_count = TestAccess<AudioRenderer>::get(renderer, &AudioRenderer::m_frame_count);
        unsigned int original_frame_count = frame_count;
        frame_count = 100;
        REQUIRE(frame_count == 100);
        
        // Reset frame_count to its original value
        frame_count = original_frame_count;
    }
    
    SECTION("Access to private member functions") {
        // Create a temporary render graph for testing
        auto final_stage = new AudioFinalRenderStage(512, 44100, 2);
        auto render_graph = new AudioRenderGraph(final_stage);
        renderer.add_render_graph(render_graph);
        
        // Call the private initialize_global_parameters method
        bool result = TestAccess<AudioRenderer>::call(renderer, &AudioRenderer::initialize_global_parameters);
        REQUIRE(result == true);
    }
}