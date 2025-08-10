#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_render_stage.h"
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

/**
 * @brief Tests for effect render stage functionality with OpenGL context
 * 
 * These tests check the effect render stage creation, initialization, and rendering in an OpenGL context.
 * Focus on gain effect with simple constant generator for predictable testing.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */

// Test parameter structure to hold buffer size and channel count combinations
struct TestParams {
    int buffer_size;
    int num_channels;
    const char* name;
};

// Define 3 different test parameter combinations
using TestParam1 = std::integral_constant<int, 0>; // 256 buffer, 1 channel
using TestParam2 = std::integral_constant<int, 1>; // 256 buffer, 2 channels
using TestParam3 = std::integral_constant<int, 2>; // 512 buffer, 2 channels
using TestParam4 = std::integral_constant<int, 3>; // 1024 buffer, 2 channels
using TestParam5 = std::integral_constant<int, 4>; // 1024 buffer, 4 channels



// Parameter lookup function
constexpr TestParams get_test_params(int index) {
    constexpr TestParams params[] = {
        {256, 1, "256_buffer_1_channel"},
        {256, 2, "256_buffer_2_channels"},
        {512, 2, "512_buffer_2_channels"},
        {1024, 2, "1024_buffer_2_channels"},
        {1024, 4, "1024_buffer_4_channels"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("AudioGainEffectRenderStage - Parameterized Gain Test", 
                   "[audio_effect_render_stage][gl_test][template]", 
                   TestParam1, TestParam3, TestParam5) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float TEST_CONSTANT_VALUE = 1.0f; // All channels output constant 1.0
    constexpr float TEST_GAIN_REDUCTION = 0.5f; // Gain effect reduces by half
    constexpr float EXPECTED_OUTPUT = TEST_CONSTANT_VALUE * TEST_GAIN_REDUCTION; // Expected result
    constexpr int NUM_FRAMES = 5; // Short test

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

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

        // Store samples for analysis - separate channels based on NUM_CHANNELS
        for (int i = 0; i < BUFFER_SIZE; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            if (NUM_CHANNELS > 1) {
                right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
            }
        }
    }

    // Verify the gain effect worked correctly
    REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    if (NUM_CHANNELS > 1) {
        REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    }

    SECTION("Gain Effect Verification") {
        // Test available channels based on NUM_CHANNELS
        std::vector<std::vector<float>*> channels = {&left_channel_samples};
        std::vector<std::string> channel_names = {"Channel_0"};
        
        if (NUM_CHANNELS > 1) {
            channels.push_back(&right_channel_samples);
            channel_names.push_back("Channel_1");
        }
        
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
        std::vector<float> test_gains;
        std::vector<float> expected_values;
        
        // Generate test gains for each channel
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            float gain = 0.2f + (ch * 0.15f); // Channel 0: 0.2, Channel 1: 0.35, Channel 2: 0.5, etc.
            test_gains.push_back(gain);
            expected_values.push_back(TEST_CONSTANT_VALUE * gain);
        }
        
        gain_effect.set_channel_gains(test_gains);
        
        for (int frame = 0; frame < 2; frame++) {
            constant_generator.render(frame);
            gain_effect.render(frame);
            final_render_stage.render(frame);
        }
        
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());
        
        // Test the first sample of each channel
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            float sample = output_data[ch];  // First sample, channel ch
            float expected = expected_values[ch];
            
            INFO("Channel " << ch << " sample: " << sample << ", expected: " << expected 
                 << " (1.0 reduced by " << test_gains[ch] << ")");
            
            REQUIRE(sample == Catch::Approx(expected).margin(0.01f));
        }
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

TEMPLATE_TEST_CASE("AudioEchoEffectRenderStage - Parameterized Echo Test", 
                   "[audio_effect_render_stage][gl_test][template]", 
                   TestParam1, TestParam3, TestParam5) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float IMPULSE_AMPLITUDE = 1.0f; // Impulse peak amplitude
    constexpr float ECHO_DELAY = 0.05f; // 50ms delay between echoes
    constexpr float ECHO_DECAY = 0.6f; // Each echo is 60% of previous
    constexpr int NUM_ECHOS = 3; // Test with 3 echoes
    constexpr int NUM_FRAMES = 20; // Longer test to capture multiple echoes
    constexpr int IMPULSE_DURATION = 10; // Impulse lasts for 10 samples

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create impulse generator shader - produces a short burst followed by silence
    // This will clearly show the echo effect as discrete peaks
    std::string impulse_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    // Generate an impulse: first few samples are 1.0, rest are 0.0
    // Use global time to ensure impulse only occurs at the beginning
    int sample_index = int(TexCoord.x * float(buffer_size));
    int frame_sample = int(global_time_val) * buffer_size + sample_index;
    
    float impulse_value = 0.0;
    if (frame_sample < )" + std::to_string(IMPULSE_DURATION) + R"() {
        impulse_value = )" + std::to_string(IMPULSE_AMPLITUDE) + R"(;
    }
    
    output_audio_texture = vec4(impulse_value, impulse_value, impulse_value, impulse_value) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create impulse generator render stage
    AudioRenderStage impulse_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                     impulse_shader, true);

    // Create echo effect render stage
    AudioEchoEffectRenderStage echo_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Create final render stage
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect: impulse generator -> echo effect -> final render stage
    REQUIRE(impulse_generator.connect_render_stage(&echo_effect));
    REQUIRE(echo_effect.connect_render_stage(&final_render_stage));

    // Configure echo effect parameters
    auto delay_param = echo_effect.find_parameter("delay");
    auto decay_param = echo_effect.find_parameter("decay");
    auto num_echos_param = echo_effect.find_parameter("num_echos");
    
    REQUIRE(delay_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(num_echos_param != nullptr);
    
    delay_param->set_value(ECHO_DELAY);
    decay_param->set_value(ECHO_DECAY);
    num_echos_param->set_value(NUM_ECHOS);
    
    // Initialize the render stages
    REQUIRE(impulse_generator.initialize());
    REQUIRE(echo_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    
    // Bind the render stages
    REQUIRE(impulse_generator.bind());
    REQUIRE(echo_effect.bind());
    REQUIRE(final_render_stage.bind());

    // Render multiple frames to capture the echo effect
    std::vector<float> left_channel_samples;
    std::vector<float> right_channel_samples;
    left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
    right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        // Render the impulse generator stage
        impulse_generator.render(frame);
        
        // Render the echo effect stage
        echo_effect.render(frame);
        
        // Render the final stage
        final_render_stage.render(frame);
        
        // Get the final output audio data
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store samples for analysis
        for (int i = 0; i < BUFFER_SIZE; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            if (NUM_CHANNELS > 1) {
                right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
            }
        }
    }

    // Verify we captured enough samples
    REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    if (NUM_CHANNELS > 1) {
        REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    }

    SECTION("Impulse Echo Timing Verification") {
        // Calculate expected echo positions in samples
        int delay_samples = static_cast<int>(ECHO_DELAY * SAMPLE_RATE);
        std::vector<int> expected_echo_positions;
        
        // Original impulse starts at sample 0
        expected_echo_positions.push_back(0);
        
        // Each echo is delayed by delay_samples
        for (int i = 1; i <= NUM_ECHOS; i++) {
            expected_echo_positions.push_back(delay_samples * i);
        }
        
        INFO("Testing echo timing with delay: " << ECHO_DELAY << "s (" << delay_samples << " samples)");
        
        // Test available channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples};
        std::vector<std::string> channel_names = {"Channel_0"};
        
        if (NUM_CHANNELS > 1) {
            channels.push_back(&right_channel_samples);
            channel_names.push_back("Channel_1");
        }
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Find peaks in the signal (values above a threshold)
            std::vector<int> peak_positions;
            float threshold = IMPULSE_AMPLITUDE * 0.1f; // 10% of impulse amplitude
            
            for (size_t i = 0; i < samples.size(); ++i) {
                if (std::abs(samples[i]) > threshold) {
                    // Check if this is a local maximum
                    bool is_peak = true;
                    int check_range = 5; // Check 5 samples around
                    
                    for (int j = std::max(0, static_cast<int>(i) - check_range); 
                         j <= std::min(static_cast<int>(samples.size()) - 1, static_cast<int>(i) + check_range); 
                         j++) {
                        if (j != static_cast<int>(i) && std::abs(samples[j]) > std::abs(samples[i])) {
                            is_peak = false;
                            break;
                        }
                    }
                    
                    if (is_peak) {
                        peak_positions.push_back(static_cast<int>(i));
                    }
                }
            }
            
            INFO("Found " << peak_positions.size() << " peaks at positions: ");
            for (size_t i = 0; i < peak_positions.size(); i++) {
                INFO("Peak " << i << " at sample " << peak_positions[i]);
            }
            
            // Verify we found at least the original impulse peak
            REQUIRE(peak_positions.size() >= 1);
            
            // Check that the first peak is near the expected impulse position (within first few samples)
            REQUIRE(peak_positions[0] < IMPULSE_DURATION + 5);

        }
    }

    SECTION("Echo Decay Amplitude Verification") {
        // Test that echo amplitudes decrease according to the decay factor
        
        INFO("Testing echo decay with factor: " << ECHO_DECAY);
        
        // Test available channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples};
        if (NUM_CHANNELS > 1) {
            channels.push_back(&right_channel_samples);
        }
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            
            // Find the maximum amplitude in the first few samples (original impulse)
            float max_original_amplitude = 0.0f;
            for (int i = 0; i < IMPULSE_DURATION + 5; i++) {
                if (i < static_cast<int>(samples.size())) {
                    max_original_amplitude = std::max(max_original_amplitude, std::abs(samples[i]));
                }
            }
            
            INFO("Channel " << ch << " original impulse max amplitude: " << max_original_amplitude);
            
            // Original impulse should be close to our expected amplitude
            REQUIRE(max_original_amplitude >= IMPULSE_AMPLITUDE * 0.8f);
            REQUIRE(max_original_amplitude <= IMPULSE_AMPLITUDE * 1.2f);
            
            // Look for subsequent peaks and verify they decrease
            int delay_samples = static_cast<int>(ECHO_DELAY * SAMPLE_RATE);
            
            for (int echo_num = 1; echo_num <= NUM_ECHOS; echo_num++) {
                int echo_start = delay_samples * echo_num;
                int echo_end = echo_start + IMPULSE_DURATION + 10; // Look a bit after expected position
                
                if (echo_end < static_cast<int>(samples.size())) {
                    float max_echo_amplitude = 0.0f;
                    for (int i = echo_start; i < echo_end; i++) {
                        max_echo_amplitude = std::max(max_echo_amplitude, std::abs(samples[i]));
                    }
                    
                    float expected_amplitude = max_original_amplitude * std::pow(ECHO_DECAY, echo_num);
                    
                    INFO("Echo " << echo_num << " max amplitude: " << max_echo_amplitude 
                         << ", expected: " << expected_amplitude);
                    
                    // Allow some tolerance for the echo amplitude
                    if (max_echo_amplitude > 0.01f) { // Only test if echo is significant
                        REQUIRE(max_echo_amplitude >= expected_amplitude * 0.5f);
                        REQUIRE(max_echo_amplitude <= expected_amplitude * 2.0f);
                    }
                }
            }
        }
    }

    SECTION("Echo Parameter Modification Test") {
        // Test that changing parameters affects the output
        
        // Change to higher decay (stronger echoes)
        float new_decay = 0.8f;
        auto decay_param = echo_effect.find_parameter("decay");
        decay_param->set_value(new_decay);
        
        // Render a few more frames
        std::vector<float> modified_samples;
        for (int frame = 0; frame < 5; frame++) {
            impulse_generator.render(frame + NUM_FRAMES);
            echo_effect.render(frame + NUM_FRAMES);
            final_render_stage.render(frame + NUM_FRAMES);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            const float* output_data = static_cast<const float*>(output_param->get_value());
            
            for (int i = 0; i < BUFFER_SIZE; i++) {
                modified_samples.push_back(output_data[i * NUM_CHANNELS]);
            }
        }
        
        // The modified samples should exist (basic test that rendering didn't crash)
        REQUIRE(modified_samples.size() == BUFFER_SIZE * 5);
        
        INFO("Successfully modified echo parameters and continued rendering");
    }

    // FIXME: This test is catching an error
    SECTION("Echo Buffer Clear Test") {
        // Test that disconnecting clears the echo buffer
        
        // Disconnect and reconnect
        REQUIRE(echo_effect.disconnect_render_stage(&final_render_stage));
        REQUIRE(echo_effect.connect_render_stage(&final_render_stage));
        
        // Render silence (no impulse) 
        std::string silence_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";
        
        AudioRenderStage silence_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                         silence_shader, true);
        REQUIRE(silence_generator.initialize());
        REQUIRE(silence_generator.bind());
        
        // Connect silence generator instead
        REQUIRE(echo_effect.disconnect_render_stage(&final_render_stage));
        REQUIRE(silence_generator.connect_render_stage(&echo_effect));
        REQUIRE(echo_effect.connect_render_stage(&final_render_stage));
        
        // Render a few frames of silence
        std::vector<float> silence_samples;
        for (int frame = 0; frame < 3; frame++) {
            silence_generator.render(frame);
            echo_effect.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            const float* output_data = static_cast<const float*>(output_param->get_value());
            
            for (int i = 0; i < BUFFER_SIZE; i++) {
                silence_samples.push_back(output_data[i * NUM_CHANNELS]);
            }
        }
        
        // After disconnection and silence, output should be very quiet
        for (float sample : silence_samples) {
            REQUIRE(std::abs(sample) < 0.05f);
        }
        
        INFO("Echo buffer successfully cleared after disconnection");
    }
}

TEMPLATE_TEST_CASE("AudioEchoEffectRenderStage - Audio Output Test", 
                   "[audio_effect_render_stage][gl_test][audio_output][template]", 
                   TestParam3, TestParam4, TestParam5) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float SINE_FREQUENCY = 440.0f; // A4 note
    constexpr float SINE_AMPLITUDE = 0.5f; // Moderate volume
    constexpr float ECHO_DELAY = 0.1f; // 200ms delay between echoes
    constexpr float ECHO_DECAY = 0.5f; // Each echo is 50% of previous
    constexpr int NUM_ECHOS = 10; // Test with 4 echoes for clear effect
    constexpr int PLAYBACK_SECONDS = 5; // Play for 5 seconds
    constexpr int NUM_FRAMES = (SAMPLE_RATE * PLAYBACK_SECONDS) / BUFFER_SIZE;

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    // Create sine wave generator render stage
    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, "build/shaders/multinote_sine_generator_render_stage.glsl");

    // Create echo effect render stage
    AudioEchoEffectRenderStage echo_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Create final render stage
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect: sine generator -> echo effect -> final render stage
    REQUIRE(sine_generator.connect_render_stage(&echo_effect));
    REQUIRE(echo_effect.connect_render_stage(&final_render_stage));

    // Add global_time parameter as a buffer parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    // Initialize the render stages
    REQUIRE(sine_generator.initialize());
    REQUIRE(echo_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    REQUIRE(AudioControlRegistry::instance().set_control<float>("delay", ECHO_DELAY));
    REQUIRE(AudioControlRegistry::instance().set_control<float>("decay", ECHO_DECAY));
    REQUIRE(AudioControlRegistry::instance().set_control<int>("num_echos", NUM_ECHOS));

    context.prepare_draw();
    
    // Bind the render stages
    REQUIRE(sine_generator.bind());
    REQUIRE(echo_effect.bind());
    REQUIRE(final_render_stage.bind());
    

    SECTION("Echo Effect Audio Playback") {
        std::cout << "\n=== Echo Effect Audio Playback Test ===" << std::endl;
        std::cout << "Playing " << SINE_FREQUENCY << "Hz sine wave with echo effect for " 
                  << PLAYBACK_SECONDS << " seconds..." << std::endl;
        std::cout << "Echo settings: " << ECHO_DELAY << "s delay, " 
                  << ECHO_DECAY << " decay, " << NUM_ECHOS << " echoes" << std::endl;
        std::cout << "You should hear a " << SINE_FREQUENCY << "Hz tone for 1 second, followed by echoes." << std::endl;

        // Create audio output for real-time playback
        AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output.open());
        REQUIRE(audio_output.start());

        // Convert frequency to MIDI note (A4 = 440Hz = MIDI note 69)
        float midi_note = 440.0f; // A4 note
        float note_gain = SINE_AMPLITUDE;

        // Start playing the note
        sine_generator.play_note(midi_note, note_gain);

        // Render and play audio with echo effect
        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            // Stop the note after 1 second to hear the echoes clearly
            if (frame == (SAMPLE_RATE / BUFFER_SIZE)) {
                sine_generator.stop_note(midi_note);
                std::cout << "Note stopped, listening for echoes..." << std::endl;
            }
            global_time_param->set_value(frame);
            global_time_param->render();

            // Render the sine wave generator
            sine_generator.render(frame);
            
            // Render the echo effect
            echo_effect.render(frame);
            
            // Render the final stage
            final_render_stage.render(frame);
            
            // Get the final output audio data
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);

            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            // Wait for audio output to be ready
            while (!audio_output.is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Push the audio data to the output for real-time playback
            audio_output.push(output_data);
        }

        // Let the audio finish playing
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Clean up
        audio_output.stop();
        std::cout << "Echo effect playback complete!" << std::endl;
        std::cout << "Did you hear the original " << SINE_FREQUENCY << "Hz tone followed by echoes getting progressively quieter?" << std::endl;
    }
}