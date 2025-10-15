#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

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
                   TestParam2, TestParam3, TestParam4) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float IMPULSE_AMPLITUDE = 1.0f; // Impulse peak amplitude
    constexpr float ECHO_DELAY = 0.1f; // 100ms delay between echoes
    constexpr float ECHO_DECAY = 0.5f; // Each echo is 60% of previous
    constexpr int NUM_ECHOS = 5; // Test with 3 echoes
    constexpr int TOTAL_SAMPLES = 50000; // Total samples to capture for echo analysis
    constexpr int NUM_FRAMES = (TOTAL_SAMPLES + BUFFER_SIZE - 1) / BUFFER_SIZE; // Calculate frames needed
    constexpr int IMPULSE_DURATION = 1; // Impulse lasts for 1 sample

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

    // Add global_time parameter as a buffer parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

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
    left_channel_samples.reserve(TOTAL_SAMPLES);
    right_channel_samples.reserve(TOTAL_SAMPLES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

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

        // Store samples for analysis (only up to TOTAL_SAMPLES)
        for (int i = 0; i < BUFFER_SIZE && left_channel_samples.size() < TOTAL_SAMPLES; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
        }
    }

    // Verify we captured the correct number of samples
    REQUIRE(left_channel_samples.size() == TOTAL_SAMPLES);
    REQUIRE(right_channel_samples.size() == TOTAL_SAMPLES);

    std::unordered_map<int, float> left_channel_echoes;
    std::unordered_map<int, float> right_channel_echoes;

    for (int i = 0; i < left_channel_samples.size(); i++) {
        if (left_channel_samples[i] > 0.01f) {
            left_channel_echoes[i] = left_channel_samples[i];
        }
    }

    for (int i = 0; i < right_channel_samples.size(); i++) {
        if (right_channel_samples[i] > 0.01f) {
            right_channel_echoes[i] = right_channel_samples[i];
        }
    }

    // Define expected echo amplitudes in chronological order (ignoring exact sample positions)
    std::vector<float> expected_amplitude_sequence = {
        1.000000f,  // Original signal
        0.500000f,  // First echo
        0.250000f,  // Second echo group
        0.250000f,
        0.125000f,  // Third echo group
        0.250000f,
        0.125000f,
        0.062500f,  // Fourth echo group
        0.187500f,
        0.187500f,
        0.062500f,
        0.031250f,  // Fifth echo group
        0.125000f,
        0.187500f,
        0.125000f,
        0.031250f
    };

    constexpr float AMPLITUDE_TOLERANCE = 0.001f; // 0.1% tolerance for floating point comparison

    // Extract amplitudes in sample order for both channels
    std::vector<float> left_amplitudes;
    std::vector<float> right_amplitudes;
    
    // Sort echoes by sample index to get chronological order
    std::vector<std::pair<int, float>> left_sorted(left_channel_echoes.begin(), left_channel_echoes.end());
    std::vector<std::pair<int, float>> right_sorted(right_channel_echoes.begin(), right_channel_echoes.end());
    
    std::sort(left_sorted.begin(), left_sorted.end());
    std::sort(right_sorted.begin(), right_sorted.end());
    
    for (const auto& echo : left_sorted) {
        left_amplitudes.push_back(echo.second);
    }
    for (const auto& echo : right_sorted) {
        right_amplitudes.push_back(echo.second);
    }

    // Verify correct number of echoes
    REQUIRE(left_amplitudes.size() == expected_amplitude_sequence.size());
    REQUIRE(right_amplitudes.size() == expected_amplitude_sequence.size());

    // Verify left channel amplitude sequence
    for (size_t i = 0; i < expected_amplitude_sequence.size(); i++) {
        REQUIRE(std::abs(left_amplitudes[i] - expected_amplitude_sequence[i]) < AMPLITUDE_TOLERANCE);
    }

    // Verify right channel amplitude sequence  
    for (size_t i = 0; i < expected_amplitude_sequence.size(); i++) {
        REQUIRE(std::abs(right_amplitudes[i] - expected_amplitude_sequence[i]) < AMPLITUDE_TOLERANCE);
    }

    // Verify both channels have identical amplitude patterns
    REQUIRE(left_amplitudes == right_amplitudes);

}

TEMPLATE_TEST_CASE("AudioEchoEffectRenderStage - Audio Output Test", 
                   "[audio_effect_render_stage][gl_test][audio_output][template]", 
                   TestParam3, TestParam4, TestParam5) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float SINE_FREQUENCY = 261.63 * std::pow(SEMI_TONE, 2);
    constexpr float SINE_AMPLITUDE = 0.3f; // Moderate volume
    constexpr float ECHO_DELAY = 0.1f; // 200ms delay between echoes
    constexpr float ECHO_DECAY = 0.4f; // Each echo is 50% of previous
    constexpr int NUM_ECHOS = 5; // Test with 4 echoes for clear effect
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

    //Configure echo effect parameters directly without using the control registry
    {
        auto delay_param = echo_effect.find_parameter("delay");
        auto decay_param = echo_effect.find_parameter("decay");
        auto num_echos_param = echo_effect.find_parameter("num_echos");
        REQUIRE(delay_param != nullptr);
        REQUIRE(decay_param != nullptr);
        REQUIRE(num_echos_param != nullptr);
        delay_param->set_value(ECHO_DELAY);
        decay_param->set_value(ECHO_DECAY);
        num_echos_param->set_value(NUM_ECHOS);
    }

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
        float midi_note = SINE_FREQUENCY; // A4 note
        float note_gain = SINE_AMPLITUDE;

        // Start playing the note
        sine_generator.play_note({midi_note, note_gain});

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
            //while (!audio_output.is_ready()) {
            //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            //}
            
            //// Push the audio data to the output for real-time playback
            //audio_output.push(output_data);
        }

        // Let the audio finish playing
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Clean up
        audio_output.stop();
        std::cout << "Echo effect playback complete!" << std::endl;
        std::cout << "Did you hear the original " << SINE_FREQUENCY << "Hz tone followed by echoes getting progressively quieter?" << std::endl;
    }
}

TEMPLATE_TEST_CASE("AudioEchoEffectRenderStage - Sequential Notes Discontinuity Test", 
                   "[audio_effect_render_stage][gl_test][audio_output][template][discontinuity]", 
                   TestParam2, TestParam3, TestParam4) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    
    // Test configuration
    constexpr float NOTE_FREQUENCIES[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f}; // C, D, E, F, G
    constexpr int NUM_TEST_NOTES = sizeof(NOTE_FREQUENCIES) / sizeof(NOTE_FREQUENCIES[0]);
    constexpr float NOTE_GAIN = 0.4f; // Moderate volume to avoid clipping
    constexpr float NOTE_DURATION = 0.2f; // Note duration in seconds
    constexpr float NOTE_GAP = 0.2f; // Gap between notes in seconds
    constexpr float ECHO_DELAY = 0.1f; // 150ms delay between echoes
    constexpr float ECHO_DECAY = 0.4f; // Each echo is 60% of previous
    constexpr int NUM_ECHOS = 5; // Number of echoes
    
    // ADSR parameters for smooth envelope
    constexpr float ATTACK_TIME = 0.1f; // 50ms smooth attack
    constexpr float DECAY_TIME = 0.1f; // 100ms decay
    constexpr float SUSTAIN_LEVEL = 0.8f; // 80% sustain level
    constexpr float RELEASE_TIME = 0.3f; // 300ms smooth release
    
    // Calculate total test duration
    constexpr float TOTAL_DURATION = NUM_TEST_NOTES * (NOTE_DURATION + NOTE_GAP) + 2.0f; // Extra time for echoes
    constexpr int TOTAL_FRAMES = static_cast<int>((SAMPLE_RATE * TOTAL_DURATION) / BUFFER_SIZE);
    
    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    
    // Create sine wave generator render stage with multinote support
    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, "build/shaders/multinote_sine_generator_render_stage.glsl");

    // Create echo effect render stage
    AudioEchoEffectRenderStage echo_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Create final render stage
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect: sine generator -> echo effect -> final render stage
    REQUIRE(sine_generator.connect_render_stage(&echo_effect));
    REQUIRE(echo_effect.connect_render_stage(&final_render_stage));

    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    // Initialize the render stages
    REQUIRE(sine_generator.initialize());
    REQUIRE(echo_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    // Configure echo effect parameters directly without using the control registry
    {
        auto delay_param2 = echo_effect.find_parameter("delay");
        auto decay_param2 = echo_effect.find_parameter("decay");
        auto num_echos_param2 = echo_effect.find_parameter("num_echos");
        REQUIRE(delay_param2 != nullptr);
        REQUIRE(decay_param2 != nullptr);
        REQUIRE(num_echos_param2 != nullptr);
        delay_param2->set_value(ECHO_DELAY);
        decay_param2->set_value(ECHO_DECAY);
        num_echos_param2->set_value(NUM_ECHOS);
    }

    // Configure ADSR envelope for smooth note transitions
    auto attack_param = sine_generator.find_parameter("attack_time");
    auto decay_param = sine_generator.find_parameter("decay_time");
    auto sustain_param = sine_generator.find_parameter("sustain_level");
    auto release_param = sine_generator.find_parameter("release_time");
    
    REQUIRE(attack_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(sustain_param != nullptr);
    REQUIRE(release_param != nullptr);
    
    attack_param->set_value(ATTACK_TIME);
    decay_param->set_value(DECAY_TIME);
    sustain_param->set_value(SUSTAIN_LEVEL);
    release_param->set_value(RELEASE_TIME);

    context.prepare_draw();
    
    // Bind the render stages
    REQUIRE(sine_generator.bind());
    REQUIRE(echo_effect.bind());
    REQUIRE(final_render_stage.bind());

    // Storage for recorded audio analysis
    std::vector<float> recorded_audio;
    std::vector<float> left_channel, right_channel;
    recorded_audio.reserve(TOTAL_FRAMES * BUFFER_SIZE * NUM_CHANNELS);
    
    // Create audio output for real-time playback
    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());
    REQUIRE(audio_output.start());

    // Timing variables
    float current_time = 0.0f;
    const float frame_duration = static_cast<float>(BUFFER_SIZE) / SAMPLE_RATE;
    int current_note_index = 0;
    bool note_playing = false;
    float note_start_time = 0.0f;
    float current_note_freq = 0.0f;
    
    std::cout << "ADSR Configuration: Attack=" << ATTACK_TIME << "s, Decay=" << DECAY_TIME 
              << "s, Sustain=" << SUSTAIN_LEVEL << ", Release=" << RELEASE_TIME << "s" << std::endl;

    // Render and analyze audio
    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        current_time = frame * frame_duration;
        
        // Note sequencing logic
        if (current_note_index < NUM_TEST_NOTES) {
            float note_sequence_start = current_note_index * (NOTE_DURATION + NOTE_GAP);
            
            // Start new note
            if (!note_playing && current_time >= note_sequence_start) {
                current_note_freq = NOTE_FREQUENCIES[current_note_index];
                sine_generator.play_note({current_note_freq, NOTE_GAIN});
                note_playing = true;
                note_start_time = current_time;
                std::cout << "Frame " << frame << " (t=" << current_time << "s): Playing note " 
                          << current_note_index + 1 << " (" << current_note_freq << "Hz)" << std::endl;
            }
            
            // Stop current note
            if (note_playing && (current_time - note_start_time) >= NOTE_DURATION) {
                sine_generator.stop_note(current_note_freq);
                note_playing = false;
                std::cout << "Frame " << frame << " (t=" << current_time << "s): Stopped note " 
                          << current_note_index + 1 << " (" << current_note_freq << "Hz)" << std::endl;
                current_note_index++;
            }
        }
        
        // Update global time and render
        global_time_param->set_value(frame);
        global_time_param->render();

        // Render all stages
        sine_generator.render(frame);
        echo_effect.render(frame);
        final_render_stage.render(frame);
        
        // Get the final output audio data
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Record audio data for analysis
        for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; i++) {
            recorded_audio.push_back(output_data[i]);
        }

        // TODO: ADD a compile flag to enable and disable output
        // Wait for audio output to be ready and play
        //while (!audio_output.is_ready()) {
        //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //}
        //audio_output.push(output_data);
    }

    // Let audio finish
    //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    audio_output.stop();
    
    // Separate stereo channels for analysis
    for (size_t i = 0; i < recorded_audio.size(); i += NUM_CHANNELS) {
        left_channel.push_back(recorded_audio[i]);
        if (NUM_CHANNELS > 1) {
            right_channel.push_back(recorded_audio[i + 1]);
        }
    }

    SECTION("Discontinuity Detection Test") {
        std::cout << "\n=== Discontinuity Detection Analysis ===" << std::endl;
        
        constexpr float DISCONTINUITY_THRESHOLD = 0.05f; // Same threshold as Python script
        std::vector<size_t> discontinuity_indices;
        std::vector<float> discontinuity_magnitudes;
        
        // Calculate sample-to-sample differences to detect discontinuities
        for (size_t i = 1; i < left_channel.size(); ++i) {
            float sample_diff = std::abs(left_channel[i] - left_channel[i - 1]);
            if (sample_diff > DISCONTINUITY_THRESHOLD) {
                discontinuity_indices.push_back(i);
                discontinuity_magnitudes.push_back(sample_diff);
            }
        }
        
        std::cout << "Discontinuity threshold: " << DISCONTINUITY_THRESHOLD << std::endl;
        std::cout << "Found " << discontinuity_indices.size() << " discontinuities" << std::endl;
        
        // Test assertion: discontinuity ratio should be reasonable
        REQUIRE(discontinuity_indices.size() == 0);
    }
    
    //SECTION("Derivative Analysis and CSV Export") {
    //    // Export audio data to CSV for visualization and analysis
    //    std::string csv_filename = "echo_test_audio_data.csv";
    //    std::ofstream csv_file(csv_filename);
    
    //    if (csv_file.is_open()) {
    //        // Write header
    //        if (NUM_CHANNELS == 1) {
    //            csv_file << "time,amplitude,note_index,note_frequency\n";
    //        } else {
    //            csv_file << "time,left_channel,right_channel,note_index,note_frequency\n";
    //        }

    //        // Calculate which note was playing at each sample
    //        const float sample_duration = 1.0f / SAMPLE_RATE;

    //        for (size_t i = 0; i < left_channel.size(); ++i) {
    //            float time_val = static_cast<float>(i) * sample_duration;

    //            // Determine which note (if any) was playing at this time
    //            int active_note_index = -1;
    //            float active_note_freq = 0.0f;

    //            for (int note_idx = 0; note_idx < NUM_TEST_NOTES; note_idx++) {
    //                float note_start_time = note_idx * (NOTE_DURATION + NOTE_GAP);
    //                float note_end_time = note_start_time + NOTE_DURATION;

    //                if (time_val >= note_start_time && time_val <= note_end_time) {
    //                    active_note_index = note_idx;
    //                    active_note_freq = NOTE_FREQUENCIES[note_idx];
    //                    break;
    //                }
    //            }

    //            // Write data to CSV
    //            csv_file << time_val << ",";
    //            if (NUM_CHANNELS == 1) {
    //                csv_file << left_channel[i] << ",";
    //            } else {
    //                csv_file << left_channel[i] << ",";
    //                if (i < right_channel.size()) {
    //                    csv_file << right_channel[i] << ",";
    //                } else {
    //                    csv_file << "0.0,";
    //                }
    //            }
    //            csv_file << active_note_index << "," << active_note_freq << "\n";
    //        }

    //        csv_file.close();
    //        std::cout << "\n=== Audio Data Export ===" << std::endl;
    //        std::cout << "Audio data exported to: " << csv_filename << std::endl;
    //        std::cout << "Total samples: " << left_channel.size() << std::endl;
    //        std::cout << "Duration: " << (left_channel.size() / static_cast<float>(SAMPLE_RATE)) << " seconds" << std::endl;
    //        std::cout << "Sample rate: " << SAMPLE_RATE << " Hz" << std::endl;
    //        std::cout << "Channels: " << NUM_CHANNELS << std::endl;
    //        std::cout << "Run 'python analyze_echo_audio.py' to visualize and analyze the audio data" << std::endl;
    //    } else {
    //        std::cout << "Failed to open " << csv_filename << " for writing" << std::endl;
    //    }

    //    std::cout << "\n=== Derivative-Based Smoothness Analysis ===" << std::endl;
    //    
    //    // Derivative-Based Smoothness Analysis
    //    const float dt = 1.0f / SAMPLE_RATE; // Time step between samples
    //    
    //    std::vector<float> derivatives;
    //    std::vector<float> second_derivatives;
    //    float max_derivative = 0.0f;
    //    float max_second_derivative = 0.0f;
    //    int sharp_edge_count = 0;
    //    
    //    // First derivative using central difference
    //    for (size_t i = 1; i < left_channel.size() - 1; ++i) {
    //        float derivative = (left_channel[i + 1] - left_channel[i - 1]) / (2.0f * dt);
    //        derivatives.push_back(derivative);
    //        max_derivative = std::max(max_derivative, std::abs(derivative));
    //    }
    //    
    //    // Second derivative (curvature analysis)
    //    for (size_t i = 1; i < derivatives.size() - 1; ++i) {
    //        float second_derivative = (derivatives[i + 1] - derivatives[i - 1]) / (2.0f * dt);
    //        second_derivatives.push_back(second_derivative);
    //        max_second_derivative = std::max(max_second_derivative, std::abs(second_derivative));
    //        
    //        // Check for sharp changes in derivative (sudden curvature changes)
    //        const float sharp_edge_threshold = 5000.0f;  // Higher threshold for audio with echo effects
    //        if (std::abs(second_derivative) > sharp_edge_threshold) {
    //            sharp_edge_count++;
    //            float time_at_edge = static_cast<float>(i + 1) / SAMPLE_RATE;
    //            std::cout << "Sharp edge detected at sample " << (i + 1) << " (t=" << time_at_edge 
    //                      << "s): second derivative = " << second_derivative << std::endl;
    //        }
    //    }
    //    
    //    std::cout << "Total samples analyzed: " << left_channel.size() << std::endl;
    //    std::cout << "Max |derivative|: " << max_derivative << std::endl;
    //    std::cout << "Max |second derivative|: " << max_second_derivative << std::endl;
    //    std::cout << "Sharp edges detected: " << sharp_edge_count << std::endl;
    //    
    //    // Export derivative analysis data to CSV
    //    std::string smoothness_csv = "echo_smoothness_analysis.csv";
    //    std::ofstream smoothness_file(smoothness_csv);
    //    if (smoothness_file.is_open()) {
    //        smoothness_file << "time,amplitude,derivative,second_derivative\n";
    //        
    //        for (size_t i = 1; i < derivatives.size() - 1 && i + 1 < left_channel.size(); ++i) {
    //            float time_val = static_cast<float>(i + 1) / SAMPLE_RATE;
    //            smoothness_file << time_val << "," << left_channel[i + 1] << "," 
    //                     << derivatives[i] << "," << second_derivatives[i] << "\n";
    //        }
    //        
    //        smoothness_file.close();
    //        std::cout << "Derivative analysis data saved to " << smoothness_csv << std::endl;
    //    }
    //    
    //}

    audio_output.close();
    
    // Cleanup
    final_render_stage.unbind();
    echo_effect.unbind();
    sine_generator.unbind();
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioFrequencyFilterEffectRenderStage - Parameterized Filter Test", 
                   "[audio_effect_render_stage][gl_test][template]", 
                   TestParam2, TestParam3, TestParam4) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float SINE_AMPLITUDE = 0.3f; // Reduced to avoid clipping with multiple notes
    constexpr float NOTE1_FREQ = 300.0f; // Below bandpass
    constexpr float NOTE2_FREQ = 700.0f; // Inside bandpass
    constexpr float NOTE3_FREQ = 1200.0f; // Above bandpass
    constexpr float LOW_CUTOFF = 500.0f;
    constexpr float HIGH_CUTOFF = 1000.0f;
    constexpr float RESONANCE = 1.0f;
    constexpr int NUM_TAPS = 101;
    constexpr float NYQUIST = SAMPLE_RATE / 2.0f;
    constexpr int TOTAL_SAMPLES = SAMPLE_RATE * 2; // 2 seconds
    constexpr int NUM_FRAMES = (TOTAL_SAMPLES + BUFFER_SIZE - 1) / BUFFER_SIZE;

    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                            "build/shaders/multinote_sine_generator_render_stage.glsl");

    AudioFrequencyFilterEffectRenderStage filter_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    filter_effect.set_low_pass(LOW_CUTOFF);
    filter_effect.set_high_pass(HIGH_CUTOFF);
    filter_effect.set_resonance(RESONANCE);
    filter_effect.set_filter_follower(0.0f);

    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    REQUIRE(sine_generator.connect_render_stage(&filter_effect));
    REQUIRE(filter_effect.connect_render_stage(&final_render_stage));

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    REQUIRE(sine_generator.initialize());
    REQUIRE(filter_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    
    REQUIRE(sine_generator.bind());
    REQUIRE(filter_effect.bind());
    REQUIRE(final_render_stage.bind());

    // Play three notes
    sine_generator.play_note({NOTE1_FREQ, SINE_AMPLITUDE});
    sine_generator.play_note({NOTE2_FREQ, SINE_AMPLITUDE});
    sine_generator.play_note({NOTE3_FREQ, SINE_AMPLITUDE});

    std::vector<float> left_channel_samples;
    std::vector<float> right_channel_samples;
    left_channel_samples.reserve(TOTAL_SAMPLES);
    if (NUM_CHANNELS > 1) right_channel_samples.reserve(TOTAL_SAMPLES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

        sine_generator.render(frame);
        filter_effect.render(frame);
        final_render_stage.render(frame);
        
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        int samples_this_frame = std::min(BUFFER_SIZE, TOTAL_SAMPLES - static_cast<int>(left_channel_samples.size()));
        for (int i = 0; i < samples_this_frame; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            if (NUM_CHANNELS > 1) {
                right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
            }
        }
    }

    REQUIRE(left_channel_samples.size() == TOTAL_SAMPLES);
    if (NUM_CHANNELS > 1) REQUIRE(right_channel_samples.size() == TOTAL_SAMPLES);

    SECTION("Filter Frequency Response Verification") {
        auto compute_power = [](const std::vector<float>& samples, float freq, float sr) -> float {
            int N = samples.size();
            std::complex<float> sum(0.0f, 0.0f);
            for (int n = 0; n < N; n++) {
                float angle = -2.0f * M_PI * freq * n / sr;
                sum += samples[n] * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            return std::abs(sum) / static_cast<float>(N);
        };

        // Analyze the second half to avoid transients
        constexpr int ANALYSIS_START = TOTAL_SAMPLES / 2;
        constexpr int ANALYSIS_SIZE = TOTAL_SAMPLES - ANALYSIS_START;
        std::vector<float> analysis_samples(left_channel_samples.begin() + ANALYSIS_START, 
                                             left_channel_samples.begin() + ANALYSIS_START + ANALYSIS_SIZE);

        float power_note1 = compute_power(analysis_samples, NOTE1_FREQ, SAMPLE_RATE);
        float power_note2 = compute_power(analysis_samples, NOTE2_FREQ, SAMPLE_RATE);
        float power_note3 = compute_power(analysis_samples, NOTE3_FREQ, SAMPLE_RATE);

        INFO("Power at " << NOTE1_FREQ << "Hz (below): " << power_note1);
        INFO("Power at " << NOTE2_FREQ << "Hz (inside): " << power_note2);
        INFO("Power at " << NOTE3_FREQ << "Hz (above): " << power_note3);

        // Note2 should have significantly higher power than Note1 and Note3
        REQUIRE(power_note2 > power_note1 * 5.0f);
        REQUIRE(power_note2 > power_note3 * 5.0f);

        if (NUM_CHANNELS > 1) {
            std::vector<float> right_analysis(right_channel_samples.begin() + ANALYSIS_START, 
                                               right_channel_samples.begin() + ANALYSIS_START + ANALYSIS_SIZE);
            float r_power_note1 = compute_power(right_analysis, NOTE1_FREQ, SAMPLE_RATE);
            float r_power_note2 = compute_power(right_analysis, NOTE2_FREQ, SAMPLE_RATE);
            float r_power_note3 = compute_power(right_analysis, NOTE3_FREQ, SAMPLE_RATE);

            REQUIRE(r_power_note2 > r_power_note1 * 5.0f);
            REQUIRE(r_power_note2 > r_power_note3 * 5.0f);
        }
    }
}

TEMPLATE_TEST_CASE("AudioFrequencyFilterEffectRenderStage - Audio Output Test", 
                   "[audio_effect_render_stage][gl_test][audio_output][template]", 
                   TestParam3, TestParam4, TestParam5) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float SINE_AMPLITUDE = 0.5f;
    constexpr float NOTE1_FREQ = 440.0f;
    constexpr float NOTE2_FREQ = 880.0f;
    constexpr float LOW_CUTOFF = 400.0f;
    constexpr float HIGH_CUTOFF = 480.0f;
    constexpr int PLAYBACK_SECONDS = 1;
    constexpr int NUM_FRAMES = (SAMPLE_RATE * PLAYBACK_SECONDS) / BUFFER_SIZE;

    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, "build/shaders/multinote_sine_generator_render_stage.glsl");

    AudioFrequencyFilterEffectRenderStage filter_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    filter_effect.set_low_pass(LOW_CUTOFF);
    filter_effect.set_high_pass(HIGH_CUTOFF);
    filter_effect.set_resonance(1.0f);
    filter_effect.set_filter_follower(0.0f);

    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    REQUIRE(sine_generator.connect_render_stage(&filter_effect));

    REQUIRE(filter_effect.connect_render_stage(&final_render_stage));

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    REQUIRE(sine_generator.initialize());
    REQUIRE(filter_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    
    REQUIRE(sine_generator.bind());
    REQUIRE(filter_effect.bind());
    REQUIRE(final_render_stage.bind());

    std::vector<float> recorded_audio;
    recorded_audio.reserve(NUM_FRAMES * BUFFER_SIZE * NUM_CHANNELS);

    std::cout << "\n=== Filter Effect Audio Playback Test ===" << std::endl;
    std::cout << "Playing two tones (" << NOTE1_FREQ << "Hz and " << NOTE2_FREQ << "Hz) with band pass filter around " << NOTE1_FREQ << "Hz for " 
              << PLAYBACK_SECONDS << " seconds..." << std::endl;
    std::cout << "You should hear only the " << NOTE1_FREQ << "Hz tone." << std::endl;

    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());
    REQUIRE(audio_output.start());

    sine_generator.play_note({NOTE1_FREQ, SINE_AMPLITUDE / 2.0f});
    sine_generator.play_note({NOTE2_FREQ, SINE_AMPLITUDE / 2.0f});

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

        sine_generator.render(frame);
        filter_effect.render(frame);
        final_render_stage.render(frame);
        
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; i++) {
            recorded_audio.push_back(output_data[i]);
        }

        //while (!audio_output.is_ready()) {
        //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //}
        //audio_output.push(output_data);
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    audio_output.stop();
    std::cout << "Filter effect playback complete!" << std::endl;

//    SECTION("Save Recorded Audio to CSV") {
//        std::ofstream csv_file("./playground/filter_output.csv");
//        if (!csv_file.is_open()) {
//            std::cerr << "Failed to open CSV file!" << std::endl;
//            return;
//        }
//        csv_file << "Sample";
//        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
//            csv_file << ",Channel" << ch;
//        }
//        csv_file << std::endl;
//        for (size_t i = 0; i < recorded_audio.size(); i += NUM_CHANNELS) {
//            csv_file << (i / NUM_CHANNELS);
//            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
//                csv_file << "," << std::fixed << std::setprecision(6) << recorded_audio[i + ch];
//            }
//            csv_file << std::endl;
//        }
//        csv_file.close();
//        std::cout << "Saved recorded audio to /workspace/playground/filter_output.csv" << std::endl;
//    }
//
    std::vector<float> left_channel;
    for (size_t i = 0; i < recorded_audio.size(); i += NUM_CHANNELS) {
        left_channel.push_back(recorded_audio[i]);
    }
}

TEMPLATE_TEST_CASE("AudioFrequencyFilterEffectRenderStage - Dynamic Parameter Changes with Square Wave and Analysis", 
                   "[audio_effect_render_stage][gl_test][audio_output][template][dynamic]", 
                   TestParam3, TestParam4, TestParam5) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float SQUARE_AMPLITUDE = 0.4f;
    constexpr float SQUARE_FREQ = 440.0f; // A4 note
    constexpr int PLAYBACK_SECONDS = 8; // 8 seconds to hear parameter changes
    constexpr int NUM_FRAMES = (SAMPLE_RATE * PLAYBACK_SECONDS) / BUFFER_SIZE;
    
    // Parameter change timing
    constexpr int PARAM_CHANGE_INTERVAL = NUM_FRAMES / 6; // Change parameters 6 times during playback
    
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Use square wave generator instead of sine wave
    AudioGeneratorRenderStage square_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, "build/shaders/multinote_square_generator_render_stage.glsl");

    AudioFrequencyFilterEffectRenderStage filter_effect(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    REQUIRE(square_generator.connect_render_stage(&filter_effect));
    REQUIRE(filter_effect.connect_render_stage(&final_render_stage));

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    REQUIRE(square_generator.initialize());
    REQUIRE(filter_effect.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    
    REQUIRE(square_generator.bind());
    REQUIRE(filter_effect.bind());
    REQUIRE(final_render_stage.bind());

    std::cout << "\n=== Dynamic Filter Parameter Test with Square Wave and Analysis ===" << std::endl;
    std::cout << "Playing " << SQUARE_FREQ << "Hz square wave for " << PLAYBACK_SECONDS << " seconds..." << std::endl;
    std::cout << "Filter parameters will change dynamically to demonstrate the effect:" << std::endl;
    std::cout << "- Low pass: 200Hz -> 2000Hz -> 200Hz" << std::endl;
    std::cout << "- High pass: 50Hz -> 500Hz -> 50Hz" << std::endl;
    std::cout << "- Resonance: 1.0 -> 3.0 -> 1.0" << std::endl;
    std::cout << "- Filter follower: 0.0 -> 0.5 -> 0.0" << std::endl;

    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());
    REQUIRE(audio_output.start());

    // Start playing the square wave
    square_generator.play_note({SQUARE_FREQ, SQUARE_AMPLITUDE});

    // Parameter change sequences
    std::vector<float> low_pass_sequence = {200.0f, 500.0f, 1000.0f, 2000.0f, 1000.0f, 500.0f, 200.0f};
    std::vector<float> high_pass_sequence = {50.0f, 100.0f, 200.0f, 500.0f, 200.0f, 100.0f, 50.0f};
    std::vector<float> resonance_sequence = {1.0f, 1.5f, 2.0f, 3.0f, 2.0f, 1.5f, 1.0f};
    std::vector<float> filter_follower_sequence = {0.0f, 0.1f, 0.2f, 0.5f, 0.2f, 0.1f, 0.0f};

    // Storage for audio analysis
    std::vector<std::vector<float>> audio_segments; // Store audio for each parameter change
    std::vector<std::string> parameter_descriptions; // Store parameter descriptions
    audio_segments.reserve(low_pass_sequence.size());
    parameter_descriptions.reserve(low_pass_sequence.size());

    // Helper function to analyze waveform characteristics
    auto analyze_waveform = [](const std::vector<float>& samples) -> std::tuple<float, float, float, float> {
        if (samples.empty()) return {0.0f, 0.0f, 0.0f, 0.0f};
        
        // Calculate RMS (Root Mean Square) - overall energy
        float rms = 0.0f;
        for (float sample : samples) {
            rms += sample * sample;
        }
        rms = std::sqrt(rms / samples.size());
        
        // Calculate peak amplitude
        float peak = 0.0f;
        for (float sample : samples) {
            peak = std::max(peak, std::abs(sample));
        }
        
        // Calculate zero crossings (rough measure of frequency content)
        int zero_crossings = 0;
        for (size_t i = 1; i < samples.size(); ++i) {
            if ((samples[i-1] >= 0.0f) != (samples[i] >= 0.0f)) {
                zero_crossings++;
            }
        }
        float zero_crossing_rate = static_cast<float>(zero_crossings) / samples.size();
        
        // Calculate spectral centroid (weighted average frequency)
        float spectral_centroid = 0.0f;
        float total_magnitude = 0.0f;
        for (size_t i = 0; i < samples.size() / 2; ++i) {
            float magnitude = std::abs(samples[i]);
            spectral_centroid += i * magnitude;
            total_magnitude += magnitude;
        }
        if (total_magnitude > 0.0f) {
            spectral_centroid = (spectral_centroid / total_magnitude) * (SAMPLE_RATE / 2.0f) / (samples.size() / 2.0f);
        }
        
        return {rms, peak, zero_crossing_rate, spectral_centroid};
    };

    // Current audio segment for analysis
    std::vector<float> current_segment;
    current_segment.reserve(PARAM_CHANGE_INTERVAL * BUFFER_SIZE * NUM_CHANNELS);
    int current_param_index = 0;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

        // Change parameters at regular intervals
        int param_index = (frame / PARAM_CHANGE_INTERVAL) % low_pass_sequence.size();
        
        if (frame % PARAM_CHANGE_INTERVAL == 0) {
            // Analyze the previous segment if we have data
            if (!current_segment.empty()) {
                audio_segments.push_back(current_segment);
                std::string desc = "Low:" + std::to_string(low_pass_sequence[current_param_index]) + 
                                 "Hz High:" + std::to_string(high_pass_sequence[current_param_index]) + 
                                 "Hz Res:" + std::to_string(resonance_sequence[current_param_index]) + 
                                 " Fol:" + std::to_string(filter_follower_sequence[current_param_index]);
                parameter_descriptions.push_back(desc);
                current_segment.clear();
            }
            
            float new_low_pass = low_pass_sequence[param_index];
            float new_high_pass = high_pass_sequence[param_index];
            float new_resonance = resonance_sequence[param_index];
            float new_filter_follower = filter_follower_sequence[param_index];
            
            filter_effect.set_low_pass(new_low_pass);
            filter_effect.set_high_pass(new_high_pass);
            filter_effect.set_resonance(new_resonance);
            filter_effect.set_filter_follower(new_filter_follower);
            
            std::cout << "Frame " << frame << " (t=" << (frame * BUFFER_SIZE / static_cast<float>(SAMPLE_RATE)) 
                      << "s): Changed filter - Low: " << new_low_pass << "Hz, High: " << new_high_pass 
                      << "Hz, Resonance: " << new_resonance << ", Follower: " << new_filter_follower << std::endl;
            
            current_param_index = param_index;
        }

        square_generator.render(frame);
        filter_effect.render(frame);
        final_render_stage.render(frame);
        
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store audio data for analysis (left channel only for simplicity)
        for (int i = 0; i < BUFFER_SIZE; i++) {
            current_segment.push_back(output_data[i * NUM_CHANNELS]);
        }

        // Wait for audio output to be ready and play
        //while (!audio_output.is_ready()) {
        //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //}
        //audio_output.push(output_data);
    }

    // Analyze the final segment
    if (!current_segment.empty()) {
        audio_segments.push_back(current_segment);
        std::string desc = "Low:" + std::to_string(low_pass_sequence[current_param_index]) + 
                         "Hz High:" + std::to_string(high_pass_sequence[current_param_index]) + 
                         "Hz Res:" + std::to_string(resonance_sequence[current_param_index]) + 
                         " Fol:" + std::to_string(filter_follower_sequence[current_param_index]);
        parameter_descriptions.push_back(desc);
    }

    // Stop the note and let audio finish
    square_generator.stop_note(SQUARE_FREQ);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    audio_output.stop();
    
    // Analyze the collected audio segments
    std::cout << "\n=== Waveform Analysis Results ===" << std::endl;
    std::cout << "Analyzing " << audio_segments.size() << " audio segments..." << std::endl;
    
    std::vector<std::tuple<float, float, float, float>> analysis_results;
    analysis_results.reserve(audio_segments.size());
    
    for (size_t i = 0; i < audio_segments.size(); ++i) {
        auto [rms, peak, zero_crossing_rate, spectral_centroid] = analyze_waveform(audio_segments[i]);
        analysis_results.push_back({rms, peak, zero_crossing_rate, spectral_centroid});
        
        std::cout << "\nSegment " << (i + 1) << " (" << parameter_descriptions[i] << "):" << std::endl;
        std::cout << "  RMS: " << std::fixed << std::setprecision(4) << rms << std::endl;
        std::cout << "  Peak: " << std::fixed << std::setprecision(4) << peak << std::endl;
        std::cout << "  Zero Crossing Rate: " << std::fixed << std::setprecision(4) << zero_crossing_rate << std::endl;
        std::cout << "  Spectral Centroid: " << std::fixed << std::setprecision(1) << spectral_centroid << " Hz" << std::endl;
    }
    
    // Verify that the waveform characteristics actually changed
    SECTION("Waveform Change Verification") {
        REQUIRE(audio_segments.size() >= 2);
        
        // Compare first and last segments to ensure changes occurred
        auto [rms1, peak1, zcr1, sc1] = analysis_results[0];
        auto [rms2, peak2, zcr2, sc2] = analysis_results[analysis_results.size() - 1];
        
        // Check that at least one characteristic changed significantly
        bool rms_changed = std::abs(rms1 - rms2) > 0.01f;
        bool peak_changed = std::abs(peak1 - peak2) > 0.01f;
        bool zcr_changed = std::abs(zcr1 - zcr2) > 0.01f;
        bool sc_changed = std::abs(sc1 - sc2) > 50.0f; // 50Hz threshold for spectral centroid
        
        INFO("RMS change: " << std::abs(rms1 - rms2) << " (threshold: 0.01)");
        INFO("Peak change: " << std::abs(peak1 - peak2) << " (threshold: 0.01)");
        INFO("Zero crossing rate change: " << std::abs(zcr1 - zcr2) << " (threshold: 0.01)");
        INFO("Spectral centroid change: " << std::abs(sc1 - sc2) << " Hz (threshold: 50)");
        
        bool any_characteristic_changed = rms_changed || peak_changed || zcr_changed || sc_changed;
        REQUIRE(any_characteristic_changed);
        
        std::cout << "\n=== Filter Effect Verification ===" << std::endl;
        std::cout << "✓ Waveform characteristics changed between segments, confirming filter effect is working!" << std::endl;
    }
    
    std::cout << "\nDynamic filter parameter test complete!" << std::endl;
    std::cout << "You should have heard the square wave's timbre change dramatically as the filter parameters were adjusted." << std::endl;
    std::cout << "The square wave's harmonics were filtered differently at each parameter change." << std::endl;
    std::cout << "The filter follower parameter made the filter respond to the audio amplitude, creating dynamic filtering effects." << std::endl;
}