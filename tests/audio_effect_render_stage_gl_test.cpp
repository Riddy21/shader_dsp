#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"

#include <iostream>
#include <vector>

/**
 * @brief Tests for effect render stage functionality with OpenGL context
 * 
 * These tests check the effect render stage creation, initialization, and rendering in an OpenGL context.
 * Focus on gain effect with simple constant generator for predictable testing.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */
constexpr int WIDTH = 512;
constexpr int HEIGHT = 2;

TEST_CASE("AudioGainEffectRenderStage - Simple Gain Test", "[audio_effect_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_CONSTANT_VALUE = 1.0f; // All channels output constant 1.0
    constexpr float TEST_GAIN_REDUCTION = 0.5f; // Gain effect reduces by half
    constexpr float EXPECTED_OUTPUT = TEST_CONSTANT_VALUE * TEST_GAIN_REDUCTION; // Expected result
    constexpr int NUM_FRAMES = 5; // Short test

    // Create test fragment shader that outputs a constant value of 1.0 for all channels
    std::string constant_shader = R"(
void main() {
    // Output constant 1.0 for all channels (this will be reduced by gain effect)
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(1.0, 1.0, 1.0, 1.0) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create constant generator render stage using string constructor (no file needed!)
    AudioRenderStage constant_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                       constant_shader, true);

    // Create gain effect render stage
    AudioGainEffectRenderStage gain_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Create final render stage
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect: generator -> gain effect -> final render stage
    REQUIRE(constant_generator.connect_render_stage(&gain_effect));
    REQUIRE(gain_effect.connect_render_stage(&final_render_stage));
    
    // No need for envelope parameters or global_time with simple render stage

    // Configure the gain effect using the convenient set_channel_gains function
    std::vector<float> channel_gains(NUM_CHANNELS, TEST_GAIN_REDUCTION);
    gain_effect.set_channel_gains(channel_gains);
    
    // Initialize the render stages
    REQUIRE(constant_generator.initialize());
    REQUIRE(gain_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    
    // Bind the render stages
    REQUIRE(constant_generator.bind());
    REQUIRE(gain_effect.bind());
    REQUIRE(final_render_stage.bind());

    // No need to play a note with simple render stage

    // Render multiple frames to test consistency
    std::vector<float> left_channel_samples;
    std::vector<float> right_channel_samples;
    left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
    right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        // Render the generator stage
        constant_generator.render(frame);
        
        // Render the gain effect stage
        gain_effect.render(frame);
        
        // Render the final stage for interpolation
        final_render_stage.render(frame);
        
        // Get the final output audio data from the final render stage
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store samples for analysis - separate left and right channels
        for (int i = 0; i < BUFFER_SIZE; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
        }
    }

    // Verify the gain effect worked correctly
    REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    SECTION("Gain Effect Verification") {
        // Test both channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples, &right_channel_samples};
        std::vector<std::string> channel_names = {"Left", "Right"};
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Check that all samples are approximately the expected output value
            for (size_t i = 0; i < samples.size(); ++i) {
                INFO("Sample " << i << " value: " << samples[i] << ", expected: " << EXPECTED_OUTPUT);
                
                // Since we have balance at 0.5 (centered), both channels should get equal signal
                REQUIRE(samples[i] == Catch::Approx(EXPECTED_OUTPUT).margin(0.01f));
            }
            
            // Test consistency - all samples should be identical for a constant input
            for (size_t i = 1; i < samples.size(); ++i) {
                REQUIRE(samples[i] == Catch::Approx(samples[0]).margin(0.001f));
            }
        }
    }

    SECTION("Per-Channel Gain Reduction Verification") {
        // Test that different channels can have different gain reductions from the constant 1.0 input
        
        // Set different reductions for different channels using the convenience function
        std::vector<float> test_gains = {0.25f}; // Channel 0: reduce to 0.25 (75% reduction)
        if (NUM_CHANNELS > 1) {
            test_gains.push_back(0.5f);  // Channel 1: reduce to 0.5 (50% reduction)  
        }
        gain_effect.set_channel_gains(test_gains);
        
        for (int frame = 0; frame < 2; frame++) {
            constant_generator.render(frame);
            gain_effect.render(frame);
            final_render_stage.render(frame);
        }
        
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());
        
        // For 2-channel audio (HEIGHT=2), test the first few samples
        // Channel 0 (y=0 to 0.5): 1.0 input reduced to 0.25
        // Channel 1 (y=0.5 to 1.0): 1.0 input reduced to 0.5
        float channel0_sample = output_data[0];  // First sample, channel 0
        float channel1_sample = output_data[1]; // First sample, channel 1
        
        INFO("Channel 0 sample: " << channel0_sample << ", expected: " << 0.25f << " (1.0 reduced by 0.25)");
        INFO("Channel 1 sample: " << channel1_sample << ", expected: " << 0.5f << " (1.0 reduced by 0.5)");
        
        REQUIRE(channel0_sample == Catch::Approx(0.25f).margin(0.01f));
        REQUIRE(channel1_sample == Catch::Approx(0.5f).margin(0.01f));
    }

    SECTION("Error Handling Verification") {
        // Test that providing too many gains is handled gracefully
        std::vector<float> too_many_gains;
        for (int i = 0; i < NUM_CHANNELS + 2; i++) {
            too_many_gains.push_back(0.5f);
        }
        
        // This should not crash and should print an error message
        // The function should return early without setting any gains
        gain_effect.set_channel_gains(too_many_gains);
        
        // Test that the original gains are still in effect
        std::vector<float> valid_gains(NUM_CHANNELS, 0.8f);
        gain_effect.set_channel_gains(valid_gains);
        
        for (int frame = 0; frame < 1; frame++) {
            constant_generator.render(frame);
            gain_effect.render(frame);
            final_render_stage.render(frame);
        }
        
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());
        
        // Should have the valid gains (0.8) applied, not the invalid ones
        float sample = output_data[0];
        REQUIRE(sample == Catch::Approx(TEST_CONSTANT_VALUE * 0.8f).margin(0.01f));
    }
}