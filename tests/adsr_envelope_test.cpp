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

/**
 * @brief Test for ADSR envelope shape collection
 * 
 * This test creates a generator render stage with a custom shader that outputs 1.0 constantly
 * and collects the ADSR envelope values to verify the envelope shape.
 */

TEST_CASE("ADSR Envelope Shape Test", "[gl][adsr][envelope]") {
    SDLWindow window(640, 480);
    GLContext context;

    const unsigned int buffer_size = 512;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 1;
    
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
        for (int i = 0; i < buffer_size; i++) { // Just getting first channel
            envelope_data.push_back(output_data[i]);
        }

        if (cycle == int(num_render_cycles * 0.5)) {
            generator.stop_note(440.0f);
        }
    }
    
    // Verify we collected data
    REQUIRE(envelope_data.size() == num_render_cycles * buffer_size);
    
    // Print envelope data for verification (can be removed later)
    std::cout << "Collected ADSR envelope data:" << std::endl;
    for (size_t i = 0; i < envelope_data.size(); ++i) {
        if (envelope_data[i] > 0.0f) {
            std::cout << "Sample " << i << ": " << envelope_data[i] << std::endl;
        }
    }
    
    generator.unbind();
    delete global_time_param;
}