#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_core/audio_control.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

// Add after existing includes
#include <complex>

/**
 * @brief Tests for AudioRenderer with echo effect functionality
 * 
 * These tests check the AudioRenderer's ability to handle echo effects with real-time audio output.
 * The test uses the AudioRenderer singleton to manage the render pipeline instead of manually
 * managing individual render stages.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */

TEST_CASE("AudioRenderer - Echo Effect Audio Output Test", 
                   "[audio_renderer][gl_test][audio_output]") {
    SKIP("Skipping test because renderer currently is a singleton and cannot be tested with multiple instances");
    
    // Get test parameters for this template instantiation
    constexpr int BUFFER_SIZE = 256;
    constexpr int NUM_CHANNELS = 2;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float SINE_FREQUENCY = 261.63 * std::pow(SEMI_TONE, 2);
    constexpr float SINE_AMPLITUDE = 0.3f; // Moderate volume
    constexpr float ECHO_DELAY = 0.1f; // 100ms delay between echoes
    constexpr float ECHO_DECAY = 0.4f; // Each echo is 40% of previous
    constexpr int NUM_ECHOS = 5; // Test with 5 echoes for clear effect
    constexpr int PLAYBACK_SECONDS = 5; // Play for 5 seconds
    constexpr int NUM_FRAMES = (SAMPLE_RATE * PLAYBACK_SECONDS) / BUFFER_SIZE;

    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);

    // Get the AudioRenderer singleton instance
    AudioRenderer& audio_renderer = AudioRenderer::get_instance();
    audio_renderer.activate_render_context();

    // Create sine wave generator render stage
    AudioGeneratorRenderStage* sine_generator = new AudioGeneratorRenderStage(
        BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
        "build/shaders/multinote_sine_generator_render_stage.glsl"
    );

    // Create echo effect render stage
    AudioEchoEffectRenderStage* echo_effect = new AudioEchoEffectRenderStage(
        BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS
    );

    // Create final render stage
    AudioFinalRenderStage* final_render_stage = new AudioFinalRenderStage(
        BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS
    );

    // Initialize the render stages
    REQUIRE(sine_generator->initialize());
    REQUIRE(echo_effect->initialize());
    REQUIRE(final_render_stage->initialize());


    // Connect: sine generator -> echo effect -> final render stage
    REQUIRE(sine_generator->connect_render_stage(echo_effect));
    REQUIRE(echo_effect->connect_render_stage(final_render_stage));

    // Create render graph starting from the final stage
    AudioRenderGraph* render_graph = new AudioRenderGraph(final_render_stage);
    
    // Add render graph to the audio renderer
    REQUIRE(audio_renderer.add_render_graph(render_graph));

    // Configure echo effect parameters using AudioControlRegistry
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"delay"}, ECHO_DELAY));
    REQUIRE(AudioControlRegistry::instance().set_control<float>({"decay"}, ECHO_DECAY));
    REQUIRE(AudioControlRegistry::instance().set_control<int>({"num_echos"}, NUM_ECHOS));

    SECTION("Echo Effect Audio Playback with AudioRenderer") {
        std::cout << "\n=== AudioRenderer Echo Effect Audio Playback Test ===" << std::endl;
        std::cout << "Playing " << SINE_FREQUENCY << "Hz sine wave with echo effect for " 
                  << PLAYBACK_SECONDS << " seconds..." << std::endl;
        std::cout << "Echo settings: " << ECHO_DELAY << "s delay, " 
                  << ECHO_DECAY << " decay, " << NUM_ECHOS << " echoes" << std::endl;
        std::cout << "You should hear a " << SINE_FREQUENCY << "Hz tone for 1 second, followed by echoes." << std::endl;

        // Create audio output for real-time playback
        AudioPlayerOutput* audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
        REQUIRE(audio_output->start());

        // Add audio output to the renderer
        REQUIRE(audio_renderer.add_render_output(audio_output));

        // Initialize the audio renderer
        REQUIRE(audio_renderer.initialize(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS));
        audio_renderer.set_lead_output(0);

        // Convert frequency to MIDI note
        float midi_note = SINE_FREQUENCY;
        float note_gain = SINE_AMPLITUDE;

        // Start playing the note
        sine_generator->play_note({midi_note, note_gain});

        // Render and play audio with echo effect using the AudioRenderer
        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            // Stop the note after 1 second to hear the echoes clearly
            if (frame == (SAMPLE_RATE / BUFFER_SIZE)) {
                sine_generator->stop_note(midi_note);
                std::cout << "Note stopped, listening for echoes..." << std::endl;
            }

            // Use AudioRenderer's render method instead of manual rendering
            audio_renderer.render();

            while (!audio_output->is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            audio_renderer.present();
        }

        // Let the audio finish playing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Clean up
        audio_output->stop();
        audio_output->close();
        std::cout << "AudioRenderer echo effect playback complete!" << std::endl;
        std::cout << "Did you hear the original " << SINE_FREQUENCY << "Hz tone followed by echoes getting progressively quieter?" << std::endl;
    }

    // Cleanup
    final_render_stage->unbind();
    echo_effect->unbind();
    sine_generator->unbind();
    
    // Note: We don't delete the renderer as it's a singleton
    // The renderer will be cleaned up when the test framework shuts down
}

TEST_CASE("AudioRenderer - Empty Audio Output Test", 
                   "[audio_renderer][gl_test][audio_output]") {
    // Get test parameters for this template instantiation
    constexpr int BUFFER_SIZE = 256;
    constexpr int NUM_CHANNELS = 2;
    constexpr int SAMPLE_RATE = 44100;
}