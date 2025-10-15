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

namespace {
    void export_envelope_smoothness_csv(
        const std::vector<float>& envelope_data,
        int sample_rate,
        const std::string& filename,
        float sharp_edge_threshold = 50000.0f
    ) {
        const float dt = 1.0f / static_cast<float>(sample_rate);

        std::vector<float> time_data;
        time_data.reserve(envelope_data.size());
        for (size_t i = 0; i < envelope_data.size(); ++i) {
            time_data.push_back(static_cast<float>(i) * dt);
        }

        std::vector<float> derivatives;
        derivatives.reserve(envelope_data.size());
        for (size_t i = 1; i + 1 < envelope_data.size(); ++i) {
            float derivative = (envelope_data[i + 1] - envelope_data[i - 1]) / (2.0f * dt);
            derivatives.push_back(derivative);
        }

        std::vector<float> second_derivatives;
        second_derivatives.reserve(derivatives.size());
        float max_derivative = 0.0f;
        float max_second_derivative = 0.0f;
        int sharp_edge_count = 0;

        for (size_t i = 1; i + 1 < derivatives.size(); ++i) {
            float second_derivative = (derivatives[i + 1] - derivatives[i - 1]) / (2.0f * dt);
            second_derivatives.push_back(second_derivative);
            max_derivative = std::max(max_derivative, std::abs(derivatives[i]));
            max_second_derivative = std::max(max_second_derivative, std::abs(second_derivative));

            if (std::abs(second_derivative) > sharp_edge_threshold) {
                sharp_edge_count++;
            }
        }

        std::ofstream csv_file(filename);
        csv_file << "time,envelope,derivative,second_derivative\n";
        // Write central region where both derivatives are defined
        // Index mapping:
        //   derivatives[j] aligns to envelope index k=j+1
        //   second_derivatives[s] aligns to envelope index k=s+2
        // Choose k in [2, N-3]
        const size_t N = envelope_data.size();
        for (size_t k = 2; k + 2 < N; ++k) {
            size_t d_idx = k - 1;   // in [1, N-4]
            size_t s_idx = k - 2;   // in [0, N-5]
            csv_file << time_data[k] << ","
                     << envelope_data[k] << ","
                     << derivatives[d_idx] << ","
                     << second_derivatives[s_idx] << "\n";
        }
        csv_file.close();

        std::cout << "CSV written: " << filename << std::endl;
        std::cout << "Sharp edges: " << sharp_edge_count
                  << ", max |d|: " << max_derivative
                  << ", max |dd|: " << max_second_derivative << std::endl;

        REQUIRE(sharp_edge_count == 0);
    }

    // Same smoothness check, but without any CSV export or stdout logging
    void assert_envelope_smoothness(
        const std::vector<float>& envelope_data,
        int sample_rate,
        float sharp_edge_threshold = 5000.0f
    ) {
        const float dt = 1.0f / static_cast<float>(sample_rate);

        std::vector<float> derivatives;
        derivatives.reserve(envelope_data.size());
        for (size_t i = 1; i + 1 < envelope_data.size(); ++i) {
            float derivative = (envelope_data[i + 1] - envelope_data[i - 1]) / (2.0f * dt);
            derivatives.push_back(derivative);
        }

        int sharp_edge_count = 0;
        for (size_t i = 1; i + 1 < derivatives.size(); ++i) {
            float second_derivative = (derivatives[i + 1] - derivatives[i - 1]) / (2.0f * dt);
            if (std::abs(second_derivative) > sharp_edge_threshold) {
                sharp_edge_count++;
            }
        }

        REQUIRE(sharp_edge_count == 0);
    }
}

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

TEMPLATE_TEST_CASE("ADSR Envelope Shape and Smoothness", "[gl][adsr][envelope][template]", 
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
    const int num_render_cycles = sample_rate/buffer_size * 5;  // Collect 5s of data

    // Play a note
    generator.play_note({440.0f, 1.0f});

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
    
    SECTION("Smoothness") {
        assert_envelope_smoothness(envelope_data, sample_rate);
    }
    SECTION("Envelope behavior") {
        // Should NOT reach full 1.0 since we stop mid-attack
        float max_envelope = *std::max_element(envelope_data.begin(), envelope_data.end());
        float final_envelope = envelope_data.back();

        REQUIRE(max_envelope > 0.95f);
        REQUIRE(final_envelope < 0.01f);
    }

    SECTION("Envelope behavior") {
        float max_envelope = *std::max_element(envelope_data.begin(), envelope_data.end());
        float final_envelope = envelope_data.back();

        REQUIRE(max_envelope > 0.95f);   // Should reach close to 1.0 during attack
        REQUIRE(final_envelope < 0.01f); // Should be near zero at the end
    }

    // FIXME: Add smoothness tests that check when a note is pressed and released in all other phases
    
    generator.unbind();
    delete global_time_param;
}

TEMPLATE_TEST_CASE("ADSR stops during Attack", "[gl][adsr][envelope][attack][template]",
                   ADSRTestParam1, ADSRTestParam2, ADSRTestParam3, ADSRTestParam4) {
    // Parameters
    constexpr auto params = get_adsr_test_params(TestType::value);
    constexpr int buffer_size = params.buffer_size;
    constexpr int num_channels = params.num_channels;
    constexpr int sample_rate = 44100;

    SDLWindow window(buffer_size, num_channels);
    GLContext context;

    // Custom fragment shader that outputs only the envelope (for debug collection)
    const std::string fragment_shader_source = R"(
        void main() {
            output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);
            debug_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

            for (int i = 0; i < active_notes; i++) {
                float start_time = calculateTimeSimple(play_positions[i]);
                float end_time = calculateTimeSimple(stop_positions[i]);
                float time = calculateTime(global_time_val, TexCoord);

                float envelope = adsr_envelope(start_time, end_time, time);
                output_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
                debug_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
            }

            output_audio_texture += texture(stream_audio_texture, TexCoord);
        }
    )";

    AudioGeneratorRenderStage generator(
        buffer_size,
        sample_rate,
        num_channels,
        fragment_shader_source,
        true
    );

    REQUIRE(generator.initialize());
    context.prepare_draw();
    REQUIRE(generator.bind());

    // ADSR params
    auto* attack_param = generator.find_parameter("attack_time");
    auto* decay_param = generator.find_parameter("decay_time");
    auto* sustain_param = generator.find_parameter("sustain_level");
    auto* release_param = generator.find_parameter("release_time");
    REQUIRE(attack_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(sustain_param != nullptr);
    REQUIRE(release_param != nullptr);

    attack_param->set_value(0.1f);   // 100 ms
    decay_param->set_value(0.2f);    // 200 ms
    sustain_param->set_value(0.7f);
    release_param->set_value(0.3f);

    const int frames_per_second = sample_rate / buffer_size;
    const int total_cycles = frames_per_second * 5; // 5 seconds

    // Global time buffer param
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Helper to run one scenario and return collected envelope
    auto run_scenario = [&](int stop_cycle) {
        std::vector<float> data;
        data.reserve(static_cast<size_t>(total_cycles) * static_cast<size_t>(buffer_size));

        // Start fresh note per scenario
        generator.play_note({440.0f, 1.0f});

        for (int cycle = 0; cycle < total_cycles; ++cycle) {
            if (cycle == stop_cycle) {
                generator.stop_note(440.0f);
            }

            global_time_param->set_value(cycle);
            global_time_param->render();
            generator.render(cycle);

            auto output_param = generator.find_parameter("debug_audio_texture");
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            for (int i = 0; i < buffer_size; ++i) {
                data.push_back(output_data[i]);
            }
        }
        return data;
    };

    // Stop halfway through attack (50 ms of 100 ms attack)
    int stop_cycle = static_cast<int>(0.05f * static_cast<float>(frames_per_second));
    auto envelope_data = run_scenario(stop_cycle);
    REQUIRE(envelope_data.size() == static_cast<size_t>(total_cycles * buffer_size));

    SECTION("Smoothness") {
        assert_envelope_smoothness(envelope_data, sample_rate);
    }
    SECTION("Envelope behavior") {
        // Should reach near 1.0 during attack before stopping mid-decay, and finish near 0
        float max_envelope = *std::max_element(envelope_data.begin(), envelope_data.end());
        float final_envelope = envelope_data.back();

        REQUIRE(max_envelope < 0.95f);
        REQUIRE(max_envelope > 0.5f);
        REQUIRE(final_envelope < 0.01f);
    }

    generator.unbind();
    delete global_time_param;
}

TEMPLATE_TEST_CASE("ADSR stops during Decay", "[gl][adsr][envelope][decay][template]",
                   ADSRTestParam1, ADSRTestParam2, ADSRTestParam3, ADSRTestParam4) {
    // Parameters
    constexpr auto params = get_adsr_test_params(TestType::value);
    constexpr int buffer_size = params.buffer_size;
    constexpr int num_channels = params.num_channels;
    constexpr int sample_rate = 44100;

    SDLWindow window(buffer_size, num_channels);
    GLContext context;

    // Custom fragment shader that outputs only the envelope (for debug collection)
    const std::string fragment_shader_source = R"(
        void main() {
            output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);
            debug_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

            for (int i = 0; i < active_notes; i++) {
                float start_time = calculateTimeSimple(play_positions[i]);
                float end_time = calculateTimeSimple(stop_positions[i]);
                float time = calculateTime(global_time_val, TexCoord);

                float envelope = adsr_envelope(start_time, end_time, time);
                output_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
                debug_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
            }

            output_audio_texture += texture(stream_audio_texture, TexCoord);
        }
    )";

    AudioGeneratorRenderStage generator(
        buffer_size,
        sample_rate,
        num_channels,
        fragment_shader_source,
        true
    );

    REQUIRE(generator.initialize());
    context.prepare_draw();
    REQUIRE(generator.bind());

    // ADSR params
    auto* attack_param = generator.find_parameter("attack_time");
    auto* decay_param = generator.find_parameter("decay_time");
    auto* sustain_param = generator.find_parameter("sustain_level");
    auto* release_param = generator.find_parameter("release_time");
    REQUIRE(attack_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(sustain_param != nullptr);
    REQUIRE(release_param != nullptr);

    attack_param->set_value(0.1f);   // 100 ms
    decay_param->set_value(0.2f);    // 200 ms
    sustain_param->set_value(0.4f);
    release_param->set_value(0.2f);

    const int frames_per_second = sample_rate / buffer_size;
    const int total_cycles = frames_per_second * 5; // 5 seconds

    // Global time buffer param
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Helper to run one scenario and return collected envelope
    auto run_scenario = [&](int stop_cycle) {
        std::vector<float> data;
        data.reserve(static_cast<size_t>(total_cycles) * static_cast<size_t>(buffer_size));

        // Start fresh note per scenario
        generator.play_note({440.0f, 1.0f});

        for (int cycle = 0; cycle < total_cycles; ++cycle) {
            if (cycle == stop_cycle) {
                generator.stop_note(440.0f);
            }

            global_time_param->set_value(cycle);
            global_time_param->render();
            generator.render(cycle);

            auto output_param = generator.find_parameter("debug_audio_texture");
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            for (int i = 0; i < buffer_size; ++i) {
                data.push_back(output_data[i]);
            }
        }
        return data;
    };

    // Stop 50 ms into decay (attack=100ms, so at 150ms total)
    int stop_cycle = static_cast<int>(0.15f * static_cast<float>(frames_per_second));
    auto envelope_data = run_scenario(stop_cycle);
    REQUIRE(envelope_data.size() == static_cast<size_t>(total_cycles * buffer_size));

    SECTION("Smoothness") {
        assert_envelope_smoothness(envelope_data, sample_rate);
    }
    SECTION("Envelope behavior") {
        // Multiple presses overlapping releases: should reach high values and still be above zero at the end
        float max_envelope = *std::max_element(envelope_data.begin(), envelope_data.end());
        float min_envelope = *std::min_element(envelope_data.begin(), envelope_data.end());
        float final_envelope = envelope_data.back();

        REQUIRE(max_envelope > 0.95f);
        REQUIRE(min_envelope >= 0.0f);
        REQUIRE(final_envelope < 0.01f);
    }

    generator.unbind();
    delete global_time_param;
}

TEMPLATE_TEST_CASE("ADSR multiple consecutive presses (edge count via bit array)",
                   "[gl][adsr][envelope][consecutive][template]",
                   ADSRTestParam1, ADSRTestParam2, ADSRTestParam3, ADSRTestParam4) {
    // Parameters
    constexpr auto params = get_adsr_test_params(TestType::value);
    constexpr int buffer_size = params.buffer_size;
    constexpr int num_channels = params.num_channels;
    constexpr int sample_rate = 44100;

    SDLWindow window(buffer_size, num_channels);
    GLContext context;

    const std::string fragment_shader_source = R"(
        void main() {
            output_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);
            debug_audio_texture = vec4(0.0, 0.0, 0.0, 0.0);

            for (int i = 0; i < active_notes; i++) {
                float start_time = calculateTimeSimple(play_positions[i]);
                float end_time = calculateTimeSimple(stop_positions[i]);
                float time = calculateTime(global_time_val, TexCoord);

                float envelope = adsr_envelope(start_time, end_time, time);
                output_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
                debug_audio_texture += vec4(envelope * gains[i], 0.0, 0.0, 0.0);
            }

            output_audio_texture += texture(stream_audio_texture, TexCoord);
        }
    )";

    AudioGeneratorRenderStage generator(
        buffer_size,
        sample_rate,
        num_channels,
        fragment_shader_source,
        true
    );

    REQUIRE(generator.initialize());
    context.prepare_draw();
    REQUIRE(generator.bind());

    // ADSR params
    auto* attack_param = generator.find_parameter("attack_time");
    auto* decay_param = generator.find_parameter("decay_time");
    auto* sustain_param = generator.find_parameter("sustain_level");
    auto* release_param = generator.find_parameter("release_time");
    REQUIRE(attack_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(sustain_param != nullptr);
    REQUIRE(release_param != nullptr);

    attack_param->set_value(0.1f);
    decay_param->set_value(0.1f);
    sustain_param->set_value(0.7f);
    release_param->set_value(0.2f);

    const int frames_per_second = sample_rate / buffer_size;
    const int total_cycles = frames_per_second * 3; // 5 seconds

    // Global time buffer param
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Plan multiple press/release cycles with each new press occurring during the previous note's release
    const int press1 = static_cast<int>(0.00f * frames_per_second);
    const int stop1  = static_cast<int>(0.10f * frames_per_second);
    const int press2 = static_cast<int>(0.15f * frames_per_second); // within release of stop1 (0.10..0.40)
    const int stop2  = static_cast<int>(0.22f * frames_per_second);
    const int press3 = static_cast<int>(0.34f * frames_per_second); // within release of stop2 (0.22..0.52)
    const int stop3  = static_cast<int>(0.44f * frames_per_second);
    const int press4 = static_cast<int>(0.56f * frames_per_second); // within release of stop3 (0.44..0.74)
    const int stop4  = static_cast<int>(0.66f * frames_per_second);
    const int press5 = static_cast<int>(0.78f * frames_per_second); // within release of stop4 (0.66..0.96)
    const int stop5  = static_cast<int>(0.88f * frames_per_second);
    const int press6 = static_cast<int>(0.96f * frames_per_second); // within release of stop5 (0.88..1.18)
    const int stop6  = static_cast<int>(0.99f * frames_per_second);

    std::vector<float> envelope_data;
    envelope_data.reserve(static_cast<size_t>(total_cycles) * static_cast<size_t>(buffer_size));

    for (int cycle = 0; cycle < total_cycles; ++cycle) {
        if (cycle == press1 || cycle == press2 || cycle == press3 ||
            cycle == press4 || cycle == press5 || cycle == press6) {
            generator.play_note({440.0f, 1.0f});
        }
        if (cycle == stop1 || cycle == stop2 || cycle == stop3 ||
            cycle == stop4 || cycle == stop5 || cycle == stop6) {
            generator.stop_note(440.0f);
        }

        global_time_param->set_value(cycle);
        global_time_param->render();
        generator.render(cycle);

        auto output_param = generator.find_parameter("debug_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);
        for (int i = 0; i < buffer_size; ++i) {
            envelope_data.push_back(output_data[i]);
        }
    }

    REQUIRE(envelope_data.size() == static_cast<size_t>(total_cycles * buffer_size));

    // Convert to bit array using a small threshold
    const float threshold = 0.001f;
    std::vector<uint8_t> active_bits;
    active_bits.reserve(envelope_data.size());
    for (float v : envelope_data) {
        active_bits.push_back(v > threshold ? 1 : 0);
    }

    SECTION("Smoothness") {
        assert_envelope_smoothness(envelope_data, sample_rate);
    }

    SECTION("Edge count") {
        int num_active_segments = 0;
        bool in_active = false;
        for (auto bit : active_bits) {
            if (bit == 1 && !in_active) {
                num_active_segments++;
                in_active = true;
            } else if (bit == 0 && in_active) {
                in_active = false;
            }
        }
        // Expect a single continuous active segment due to overlapping releases
        REQUIRE(num_active_segments == 1);
    }

    SECTION("Peak count") {
        int num_peaks = 0;
        const float min_peak_height = 0.01f; // Minimum peak height to avoid noise
        const int min_peak_distance = 5; // Minimum samples between peaks
        
        // Calculate derivative using central difference
        std::vector<float> derivative;
        derivative.reserve(envelope_data.size() - 2);
        
        for (size_t i = 1; i < envelope_data.size() - 1; ++i) {
            derivative.push_back((envelope_data[i + 1] - envelope_data[i - 1]) / 2.0f);
        }
        
        // Find peaks where derivative changes from positive to negative
        for (size_t i = 1; i < derivative.size() - 1; ++i) {
            if (derivative[i - 1] > 0 && derivative[i + 1] < 0 && 
                envelope_data[i + 1] > min_peak_height) { // i+1 because derivative is offset by 1
                num_peaks++;
                // Skip ahead to avoid detecting the same peak multiple times
                i += min_peak_distance;
            }
        }
        REQUIRE(num_peaks == 6);
    }

    generator.unbind();
    delete global_time_param;
}