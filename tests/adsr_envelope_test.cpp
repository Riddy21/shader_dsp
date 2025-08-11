#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "utilities/egl_compatibility.h"

#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

#include <vector>
#include <iostream>
#include <cmath>
#include <fstream>
#include <algorithm>

/**
 * @brief Test for ADSR envelope shape collection
 * 
 * This test creates a generator render stage with a custom shader that outputs 1.0 constantly
 * and collects the ADSR envelope values to verify the envelope shape.
 */

// Test parameter structure to hold buffer size and channel count combinations
struct ADSRTestParams {
    int buffer_size;
    int num_channels;
    const char* name;
};

// Define different test parameter combinations for ADSR test
using ADSRTestParam1 = std::integral_constant<int, 0>; // 256 buffer, 1 channel
using ADSRTestParam2 = std::integral_constant<int, 1>; // 512 buffer, 2 channels
using ADSRTestParam3 = std::integral_constant<int, 2>; // 1024 buffer, 2 channels
using ADSRTestParam4 = std::integral_constant<int, 3>; // 512 buffer, 4 channels

// Parameter lookup function for ADSR test
constexpr ADSRTestParams get_adsr_test_params(int index) {
    constexpr ADSRTestParams params[] = {
        {256, 1, "256_buffer_1_channel"},
        {512, 2, "512_buffer_2_channel"},
        {1024, 2, "1024_buffer_2_channels"},
        {512, 4, "512_buffer_4_channels"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("ADSR Envelope Shape Test", "[gl][adsr][envelope][template]", 
                   ADSRTestParam1, ADSRTestParam2, ADSRTestParam3, ADSRTestParam4) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_adsr_test_params(TestType::value);
    constexpr int buffer_size = params.buffer_size;
    constexpr int num_channels = params.num_channels;
    constexpr int sample_rate = 44100;
    
    SDLWindow window(buffer_size, num_channels);
    GLContext context;
    
         // Custom fragment shader that outputs 1.0 constantly and multiplies by ADSR envelope
     const std::string fragment_shader_source = R"(
         void main() {
             output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);
             debug_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

             for (int i = 0; i < active_notes; i++) {
                 float start_time = calculateTimeSimple(play_positions[i]);
                 float end_time = calculateTimeSimple(stop_positions[i]);
                 float time = calculateTime(global_time_val, TexCoord);
                 
                 // Get envelope value
                 float envelope = adsr_envelope(start_time, end_time, time);
                 
                 // Output constant 1.0 multiplied by envelope
                 float output_sample = 1.0 * envelope;
                 
                 // Add to output (multiply by gain for completeness)
                 output_audio_texture += vec4(output_sample * gains[i], 0.0, 0.0, 0.0);
                 
                 // Store envelope in debug texture for collection
                 debug_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
             }
             
             output_audio_texture += texture(stream_audio_texture, TexCoord);
         }
     )";

    // Create generator with string-based constructor
    AudioGeneratorRenderStage generator(
        buffer_size, 
        sample_rate, 
        num_channels,
        fragment_shader_source,
        true // use_shader_string
    );

    // Initialize the generator
    REQUIRE(generator.initialize());

    context.prepare_draw();

    // Bind and render
    REQUIRE(generator.bind());
    
    // Set ADSR parameters for a clear envelope shape
    auto* attack_param = generator.find_parameter("attack_time");
    auto* decay_param = generator.find_parameter("decay_time");
    auto* sustain_param = generator.find_parameter("sustain_level");
    auto* release_param = generator.find_parameter("release_time");
    
    REQUIRE(attack_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(sustain_param != nullptr);
    REQUIRE(release_param != nullptr);
    
    // Set envelope parameters
    attack_param->set_value(0.1f);   // 100ms attack
    decay_param->set_value(0.2f);    // 200ms decay
    sustain_param->set_value(0.7f);  // 70% sustain level
    release_param->set_value(0.3f);  // 300ms release
    
    std::vector<float> envelope_data;
    const int num_render_cycles = sample_rate/buffer_size;  // Collect 1s of data

    // Play a note
    generator.play_note(440.0f, 1.0f);

    // Add global_time parameter as a buffer parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    
    for (int cycle = 0; cycle < num_render_cycles; ++cycle) {
        global_time_param->set_value(cycle);
        global_time_param->render();

        // Render current frame
        generator.render(cycle);
        
        // Read back the debug texture (contains envelope values)
        // Get the final output audio data from the final render stage
        auto output_param = generator.find_parameter("debug_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);
        // Only test the first channel regardless of total number of channels
        for (int i = 0; i < buffer_size; i++) {
            envelope_data.push_back(output_data[i]); // Only collect channel 0 data
        }

        if (cycle == int(num_render_cycles * 0.5)) {
            generator.stop_note(440.0f);
        }
    }
    
    // Verify we collected data (only from first channel)
    const size_t expected_samples = num_render_cycles * buffer_size;
    REQUIRE(envelope_data.size() == expected_samples);
    
    SECTION("Sharp Edge Detection and Data Output") {
        std::cout << "\n=== ADSR Envelope Smoothness Analysis ===" << std::endl;
        
        // Create time data for analysis
        std::vector<float> time_data;
        const float dt = 1.0f / sample_rate;
        for (size_t i = 0; i < envelope_data.size(); ++i) {
            time_data.push_back(i * dt);
        }
        
        // Output data to CSV file for Python visualization
        std::string filename = "envelope_analysis_data.csv";
        std::ofstream csv_file(filename);
        csv_file << "time,envelope,derivative,second_derivative\n";
        
        // Compute derivatives and check for sharp edges
        std::vector<float> derivatives;
        std::vector<float> second_derivatives;
        float max_derivative = 0.0f;
        float max_second_derivative = 0.0f;
        int sharp_edge_count = 0;
        
        // First derivative using central difference
        for (size_t i = 1; i < envelope_data.size() - 1; ++i) {
            float derivative = (envelope_data[i + 1] - envelope_data[i - 1]) / (2.0f * dt);
            derivatives.push_back(derivative);
            max_derivative = std::max(max_derivative, std::abs(derivative));
        }
        
        // Second derivative (curvature analysis)
        for (size_t i = 1; i < derivatives.size() - 1; ++i) {
            float second_derivative = (derivatives[i + 1] - derivatives[i - 1]) / (2.0f * dt);
            second_derivatives.push_back(second_derivative);
            max_second_derivative = std::max(max_second_derivative, std::abs(second_derivative));
            
            // Check for sharp changes in derivative (sudden curvature changes)
            const float sharp_edge_threshold = 1000.0f;  // Adjust based on requirements
            
            if (std::abs(second_derivative) > sharp_edge_threshold) {
                // Calculate the actual value change around this point
                size_t envelope_idx = i + 1;  // Corresponding envelope data index
                float value_change = 0.0f;
                
                // Look at a small window around this point to determine value change
                if (envelope_idx >= 2 && envelope_idx < envelope_data.size() - 2) {
                    float before_value = envelope_data[envelope_idx - 2];
                    float after_value = envelope_data[envelope_idx + 2];
                    value_change = std::abs(after_value - before_value);
                }
                
                sharp_edge_count++;
                std::cout << "Sharp edge detected at time " << time_data[envelope_idx] 
                          << "s, second derivative: " << second_derivative 
                          << ", value change: " << value_change << std::endl;
            }
            
            // Write to CSV (offset indices to match derivative arrays)
            if (i + 1 < time_data.size() && i < derivatives.size()) {
                csv_file << time_data[i + 1] << "," << envelope_data[i + 1] << "," 
                         << derivatives[i] << "," << second_derivative << "\n";
            }
        }
        
        csv_file.close();
        std::cout << "Envelope analysis data written to " << filename << std::endl;
        
        // Smoothness verification and reporting
        std::cout << "\n=== Smoothness Analysis Results ===" << std::endl;
        std::cout << "Total samples analyzed: " << envelope_data.size() << std::endl;
        std::cout << "Max envelope value: " << *std::max_element(envelope_data.begin(), envelope_data.end()) << std::endl;
        std::cout << "Max |derivative|: " << max_derivative << std::endl;
        std::cout << "Max |second derivative|: " << max_second_derivative << std::endl;
        std::cout << "Sharp edges detected: " << sharp_edge_count << std::endl;
        
        // Verify envelope behavior
        float max_envelope = *std::max_element(envelope_data.begin(), envelope_data.end());
        float final_envelope = envelope_data.back();
        
        // Find non-zero envelope region
        size_t first_nonzero = 0, last_nonzero = 0;
        for (size_t i = 0; i < envelope_data.size(); ++i) {
            if (envelope_data[i] > 0.001f) {
                if (first_nonzero == 0) first_nonzero = i;
                last_nonzero = i;
            }
        }
        
        std::cout << "Active envelope duration: " << (last_nonzero - first_nonzero) * dt << " seconds" << std::endl;
        
        // Assert smoothness criteria
        REQUIRE(sharp_edge_count == 0);  // No sharp edges should be detected
        REQUIRE(max_envelope > 0.95f);   // Should reach close to 1.0 during attack
        REQUIRE(final_envelope < 0.01f); // Should be near zero at the end
        
        std::cout << "✓ All smoothness tests passed!" << std::endl;
        std::cout << "Run 'python visualize_envelope.py envelope_analysis_data.csv' to visualize the results." << std::endl;
    }

    // FIXME: Add smoothness tests that check when a note is pressed and released in all other phases
    
    generator.unbind();
    delete global_time_param;
}