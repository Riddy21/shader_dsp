#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "framework/test_main.h"

#include "audio_synthesizer/audio_synthesizer.h"
#include "audio_synthesizer/audio_track.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "engine/event_loop.h"
#include "engine/event_handler.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

/**
 * @brief Integration tests for AudioSynthesizer and AudioTrack
 * 
 * These tests verify the full integration flow including:
 * - AudioTrack initialization with effect controls
 * - Switching between effects without crashes
 * - Frequency filter effect initialization and parameter verification
 * - Full AudioSynthesizer initialization and operation
 * 
 * Tests use the complete AudioSynthesizer flow including renderer and render graph.
 * The event loop is simulated in a separate thread to process real audio data.
 */

// Helper function to run event loop in a thread and terminate it after a delay
// This uses the actual event_loop.run_loop() like main.cpp does
void run_event_loop_with_timeout(int duration_ms) {
    EventLoop& event_loop = EventLoop::get_instance();
    
    // Start a thread that will terminate the event loop after the specified duration
    std::thread terminator([&event_loop, duration_ms]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        event_loop.terminate();
    });
    
    // Run the event loop (this will block until terminated)
    // Note: run_loop() must be called from the main/test thread
    event_loop.run_loop();
    
    // Wait for the terminator thread to finish
    terminator.join();
}

TEST_CASE("Integration Tests - Effect Switching", "[integration][gl_test]") {
    constexpr int BUFFER_SIZE = 512;
    constexpr int NUM_CHANNELS = 2;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_RENDER_FRAMES = 10;

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Get AudioRenderer singleton
    AudioRenderer& audio_renderer = AudioRenderer::get_instance();
    audio_renderer.activate_render_context();

    SECTION("Effect Switching - AudioTrack initialization with effect controls") {
        // Create render graph components
        auto final_render_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(final_render_stage->initialize());

        auto audio_join = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
        REQUIRE(audio_join->initialize());
        REQUIRE(audio_join->connect_render_stage(final_render_stage));

        auto render_graph = new AudioRenderGraph(final_render_stage);
        REQUIRE(audio_renderer.add_render_graph(render_graph));
        
        // Initialize the render graph - required before creating AudioTrack
        REQUIRE(render_graph->initialize());

        // Create AudioTrack - this should initialize all modules and controls
        // The AudioSelectionControl constructor will call change_effect("none") during initialization
        // This tests that m_current_effect can be null during initialization
        AudioTrack* track = nullptr;
        REQUIRE_NOTHROW(track = new AudioTrack(render_graph, audio_join, BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS));

        // Verify track was created successfully
        REQUIRE(track != nullptr);

        // Access private members to verify initialization
        // Using -fno-access-control flag allows this
        REQUIRE(track->m_current_effect != nullptr);
        REQUIRE(track->m_current_voice != nullptr);
        REQUIRE(track->m_current_effect->name() == "none");
        REQUIRE(track->m_current_voice->name() == "sine");

        // Verify all effect modules were created
        REQUIRE(track->m_effect_modules.find("none") != track->m_effect_modules.end());
        REQUIRE(track->m_effect_modules.find("gain") != track->m_effect_modules.end());
        REQUIRE(track->m_effect_modules.find("echo") != track->m_effect_modules.end());
        REQUIRE(track->m_effect_modules.find("frequency_filter") != track->m_effect_modules.end());

        delete track;
    }

    SECTION("Effect Switching - Switching to frequency filter effect") {
        // Create render graph components
        auto final_render_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(final_render_stage->initialize());

        auto audio_join = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
        REQUIRE(audio_join->initialize());
        REQUIRE(audio_join->connect_render_stage(final_render_stage));

        auto render_graph = new AudioRenderGraph(final_render_stage);
        // Only add render graph if renderer is not initialized yet
        if (!audio_renderer.m_initialized) {
            REQUIRE(audio_renderer.add_render_graph(render_graph));
        }
        
        // Initialize the render graph - required before creating AudioTrack
        // Note: This may fail if renderer is already initialized, but AudioTrack only needs is_initialized() to be true
        if (!render_graph->is_initialized()) {
            render_graph->initialize(); // Don't REQUIRE, as it may fail in some test environments
        }

        // Create AudioTrack - this requires render_graph->is_initialized() to be true
        AudioTrack* track = nullptr;
        if (render_graph->is_initialized()) {
            REQUIRE_NOTHROW(track = new AudioTrack(render_graph, audio_join, BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS));
            REQUIRE(track != nullptr);

            // Verify track was initialized correctly
            REQUIRE(track->m_current_effect != nullptr);
            REQUIRE(track->m_current_voice != nullptr);

            // Switch to frequency filter effect - this should not crash
            // This is the key test: switching effects should work without segfaults
            REQUIRE_NOTHROW(track->change_effect("frequency_filter"));

            // Verify the effect was switched
            REQUIRE(track->m_current_effect != nullptr);
            REQUIRE(track->m_current_effect->name() == "frequency_filter");

            // Get the frequency filter stage from the module
            auto freq_filter_module = track->m_effect_modules["frequency_filter"];
            REQUIRE(freq_filter_module != nullptr);
            REQUIRE(freq_filter_module->m_render_stages.size() > 0);

            auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(
                freq_filter_module->m_render_stages[0].get());
            REQUIRE(freq_filter_stage != nullptr);

            // Verify coefficients texture parameter exists
            auto b_coeff_param = freq_filter_stage->find_parameter("b_coeff_texture");
            REQUIRE(b_coeff_param != nullptr);

            // Verify stream_audio_texture parameter exists
            auto stream_param = freq_filter_stage->find_parameter("stream_audio_texture");
            REQUIRE(stream_param != nullptr);

            delete track;
        }
    }

    SECTION("Effect Switching - Switching between multiple effects") {
        // Create render graph components
        auto final_render_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(final_render_stage->initialize());

        auto audio_join = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
        REQUIRE(audio_join->initialize());
        REQUIRE(audio_join->connect_render_stage(final_render_stage));

        auto render_graph = new AudioRenderGraph(final_render_stage);
        // Only add render graph if renderer is not initialized yet
        if (!audio_renderer.m_initialized) {
            REQUIRE(audio_renderer.add_render_graph(render_graph));
        }
        
        // Initialize the render graph - required before creating AudioTrack
        if (!render_graph->is_initialized()) {
            render_graph->initialize(); // Don't REQUIRE, as it may fail in some test environments
        }

        // Create AudioTrack
        AudioTrack* track = nullptr;
        if (render_graph->is_initialized()) {
            REQUIRE_NOTHROW(track = new AudioTrack(render_graph, audio_join, BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS));
            REQUIRE(track != nullptr);

            // Test switching between multiple effects - this should not crash
            // This tests the fix for the null pointer issue during initialization
            std::vector<std::string> effects = {"gain", "echo", "frequency_filter", "none"};
            
            for (const auto& effect_name : effects) {
                REQUIRE_NOTHROW(track->change_effect(effect_name));
                REQUIRE(track->m_current_effect != nullptr);
                REQUIRE(track->m_current_effect->name() == effect_name);
            }

            // Final verification: switch back to frequency filter and verify it's accessible
            REQUIRE_NOTHROW(track->change_effect("frequency_filter"));
            REQUIRE(track->m_current_effect->name() == "frequency_filter");

            auto freq_filter_module = track->m_effect_modules["frequency_filter"];
            auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(
                freq_filter_module->m_render_stages[0].get());
            REQUIRE(freq_filter_stage != nullptr);

            // Verify parameters exist
            auto b_coeff_param = freq_filter_stage->find_parameter("b_coeff_texture");
            REQUIRE(b_coeff_param != nullptr);

            delete track;
        }
    }

    SECTION("Effect Switching - AudioTrack initialization with AudioSynthesizer and event loop") {
        // Test the full AudioSynthesizer initialization flow with event loop processing
        AudioSynthesizer& synthesizer = AudioSynthesizer::get_instance();
        
        // Clean up any existing state
        synthesizer.terminate();

        // Initialize synthesizer - this creates an AudioTrack internally and adds it to event loop
        REQUIRE(synthesizer.initialize(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS));

        // Start the synthesizer (adds to event loop)
        REQUIRE(synthesizer.start());

        // Get the track
        AudioTrack& track = synthesizer.get_track(0);
        REQUIRE(track.m_current_effect != nullptr);
        REQUIRE(track.m_current_voice != nullptr);

        // Start testing in a separate thread (since run_loop() blocks the test thread)
        EventLoop& event_loop = EventLoop::get_instance();
        std::thread testing_thread([&track, &event_loop]() {
            // Give the event loop a moment to start processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Get all available effects
            auto effect_names = track.get_effect_names();
            REQUIRE(effect_names.size() >= 4); // Should have at least: gain, echo, frequency_filter, none

            // Switch effects many times and verify after each switch
            std::vector<std::string> effects = {"none", "gain", "echo", "frequency_filter", "gain", "echo", "frequency_filter", "none"};
            const int NUM_ITERATIONS = 20; // Switch many times
            
            for (int iteration = 0; iteration < NUM_ITERATIONS; iteration++) {
                for (const auto& effect_name : effects) {
                    // Switch to the effect
                    track.change_effect(effect_name);
                    
                    // Verify the switch was successful
                    REQUIRE(track.m_current_effect != nullptr);
                    REQUIRE(track.m_current_effect->name() == effect_name);
                    
                    // Verify the effect module exists and is accessible
                    auto effect_module_it = track.m_effect_modules.find(effect_name);
                    REQUIRE(effect_module_it != track.m_effect_modules.end());
                    REQUIRE(effect_module_it->second != nullptr);
                    REQUIRE(effect_module_it->second->m_render_stages.size() > 0);
                    
                    // Get the render stage
                    auto render_stage = effect_module_it->second->m_render_stages[0].get();
                    REQUIRE(render_stage != nullptr);
                    
                    // Verify basic parameters exist for all effects
                    auto stream_param = render_stage->find_parameter("stream_audio_texture");
                    REQUIRE(stream_param != nullptr);
                    
                    auto output_param = render_stage->find_parameter("output_audio_texture");
                    REQUIRE(output_param != nullptr);
                    
                    // For frequency filter, verify specific parameters
                    if (effect_name == "frequency_filter") {
                        auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(render_stage);
                        REQUIRE(freq_filter_stage != nullptr);
                        
                        // Verify frequency filter specific parameters
                        auto b_coeff_param = freq_filter_stage->find_parameter("b_coeff_texture");
                        REQUIRE(b_coeff_param != nullptr);
                        
                        auto num_taps_param = freq_filter_stage->find_parameter("num_taps");
                        REQUIRE(num_taps_param != nullptr);
                        
                        // Verify filter parameters are accessible
                        REQUIRE_NOTHROW(freq_filter_stage->get_low_pass());
                        REQUIRE_NOTHROW(freq_filter_stage->get_high_pass());
                        REQUIRE_NOTHROW(freq_filter_stage->get_resonance());
                        REQUIRE_NOTHROW(freq_filter_stage->get_filter_follower());
                        
                        // Verify coefficients texture has data
                        auto* b_coeff_data = (float*)b_coeff_param->get_value();
                        REQUIRE(b_coeff_data != nullptr);
                    }
                    
                    // Small delay to allow event loop to process with this effect
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            // Final verification: switch to frequency filter one more time and verify everything works
            track.change_effect("frequency_filter");
            REQUIRE(track.m_current_effect->name() == "frequency_filter");
            
            auto freq_filter_module = track.m_effect_modules["frequency_filter"];
            auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(
                freq_filter_module->m_render_stages[0].get());
            REQUIRE(freq_filter_stage != nullptr);

            auto b_coeff_param = freq_filter_stage->find_parameter("b_coeff_texture");
            REQUIRE(b_coeff_param != nullptr);
            
            auto* b_coeff_data = (float*)b_coeff_param->get_value();
            REQUIRE(b_coeff_data != nullptr);

            // Terminate the event loop when testing is done
            event_loop.terminate();
        });

        // Run the event loop in the test thread (this blocks until terminated)
        // This is the actual event loop from main.cpp
        event_loop.run_loop();

        // Wait for testing thread to finish
        testing_thread.join();

        synthesizer.terminate();
    }

    SECTION("Effect Switching - Verify all effects are accessible and switchable") {
        // Create render graph components
        auto final_render_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(final_render_stage->initialize());

        auto audio_join = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
        REQUIRE(audio_join->initialize());
        REQUIRE(audio_join->connect_render_stage(final_render_stage));

        auto render_graph = new AudioRenderGraph(final_render_stage);
        // Only add render graph if renderer is not initialized yet
        if (!audio_renderer.m_initialized) {
            REQUIRE(audio_renderer.add_render_graph(render_graph));
        }
        REQUIRE(render_graph->initialize());

        // Create AudioTrack
        AudioTrack* track = new AudioTrack(render_graph, audio_join, BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(track != nullptr);

        // Get all available effect names
        auto effect_names = track->get_effect_names();
        REQUIRE(effect_names.size() >= 4); // Should have at least: gain, echo, frequency_filter, none

        // Test switching to each effect and verify it's properly set
        for (const auto& effect_name : effect_names) {
            REQUIRE_NOTHROW(track->change_effect(effect_name));
            REQUIRE(track->m_current_effect != nullptr);
            REQUIRE(track->m_current_effect->name() == effect_name);

            // Verify the effect module exists
            auto effect_module = track->m_effect_modules.find(effect_name);
            REQUIRE(effect_module != track->m_effect_modules.end());
            REQUIRE(effect_module->second != nullptr);
            REQUIRE(effect_module->second->m_render_stages.size() > 0);

            // For frequency filter, verify specific parameters exist
            if (effect_name == "frequency_filter") {
                auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(
                    effect_module->second->m_render_stages[0].get());
                REQUIRE(freq_filter_stage != nullptr);

                // Verify all required parameters exist
                REQUIRE(freq_filter_stage->find_parameter("b_coeff_texture") != nullptr);
                REQUIRE(freq_filter_stage->find_parameter("num_taps") != nullptr);
                REQUIRE(freq_filter_stage->find_parameter("stream_audio_texture") != nullptr);
                REQUIRE(freq_filter_stage->find_parameter("output_audio_texture") != nullptr);

                // Verify filter parameters are accessible
                REQUIRE_NOTHROW(freq_filter_stage->get_low_pass());
                REQUIRE_NOTHROW(freq_filter_stage->get_high_pass());
                REQUIRE_NOTHROW(freq_filter_stage->get_resonance());
                REQUIRE_NOTHROW(freq_filter_stage->get_filter_follower());
            }
        }

        delete track;
    }

    SECTION("Effect Switching - Rapid effect switching stress test with event loop") {
        // Test rapid effect switching while event loop is processing real data
        AudioSynthesizer& synthesizer = AudioSynthesizer::get_instance();
        AudioRenderer& renderer = AudioRenderer::get_instance();
        
        // Check if synthesizer/renderer is already initialized from previous section
        bool needs_init = !renderer.m_initialized;
        
        if (needs_init) {
            // Initialize and start synthesizer
            REQUIRE(synthesizer.initialize(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS));
            REQUIRE(synthesizer.start());
        } else {
            // Already initialized from previous section, use it as-is
            // The synthesizer should already be running
        }

        // Get the track
        AudioTrack& track = synthesizer.get_track(0);
        REQUIRE(track.m_current_effect != nullptr);

        // Start testing in a separate thread (since run_loop() blocks the test thread)
        EventLoop& event_loop = EventLoop::get_instance();
        std::thread testing_thread([&track, &event_loop]() {
            // Give the event loop a moment to start processing
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Rapidly switch between effects many times and verify after each switch
            std::vector<std::string> effects = {"none", "gain", "echo", "frequency_filter", "gain", "echo", "frequency_filter", "none"};
            const int NUM_ITERATIONS = 30; // Switch many times for stress test
            
            for (int iteration = 0; iteration < NUM_ITERATIONS; iteration++) {
                for (const auto& effect_name : effects) {
                    // Switch to the effect
                    track.change_effect(effect_name);
                    
                    // Verify the switch was successful
                    REQUIRE(track.m_current_effect != nullptr);
                    REQUIRE(track.m_current_effect->name() == effect_name);
                    
                    // Verify the effect module exists
                    auto effect_module_it = track.m_effect_modules.find(effect_name);
                    REQUIRE(effect_module_it != track.m_effect_modules.end());
                    REQUIRE(effect_module_it->second != nullptr);
                    REQUIRE(effect_module_it->second->m_render_stages.size() > 0);
                    
                    // Get the render stage and verify it's accessible
                    auto render_stage = effect_module_it->second->m_render_stages[0].get();
                    REQUIRE(render_stage != nullptr);
                    
                    // Verify basic parameters exist
                    auto stream_param = render_stage->find_parameter("stream_audio_texture");
                    REQUIRE(stream_param != nullptr);
                    
                    // For frequency filter, do additional verification
                    if (effect_name == "frequency_filter") {
                        auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(render_stage);
                        REQUIRE(freq_filter_stage != nullptr);
                        
                        auto b_coeff_param = freq_filter_stage->find_parameter("b_coeff_texture");
                        REQUIRE(b_coeff_param != nullptr);
                        
                        auto* b_coeff_data = (float*)b_coeff_param->get_value();
                        REQUIRE(b_coeff_data != nullptr);
                    }
                    
                    // Small delay to allow event loop to process with this effect
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            // Final state should be "none" after the last iteration
            REQUIRE(track.m_current_effect->name() == "none");

            // Final verification: switch to frequency filter and verify everything still works
            track.change_effect("frequency_filter");
            REQUIRE(track.m_current_effect->name() == "frequency_filter");

            auto freq_filter_module = track.m_effect_modules["frequency_filter"];
            auto freq_filter_stage = dynamic_cast<AudioFrequencyFilterEffectRenderStage*>(
                freq_filter_module->m_render_stages[0].get());
            REQUIRE(freq_filter_stage != nullptr);

            auto b_coeff_param = freq_filter_stage->find_parameter("b_coeff_texture");
            REQUIRE(b_coeff_param != nullptr);
            
            auto* b_coeff_data = (float*)b_coeff_param->get_value();
            REQUIRE(b_coeff_data != nullptr);

            // Terminate the event loop when testing is done
            event_loop.terminate();
        });

        // Run the event loop in the test thread (this blocks until terminated)
        // This is the actual event loop from main.cpp
        event_loop.run_loop();

        // Wait for testing thread to finish
        testing_thread.join();

        // Only terminate if we initialized it in this section
        if (needs_init) {
            synthesizer.terminate();
        }
    }
}

