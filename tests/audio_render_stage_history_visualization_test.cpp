#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#define private public
#include "audio_render_stage/audio_render_stage_history.h"
#undef private

#include "audio_core/audio_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "utilities/shader_program.h"
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <memory>

// Create a custom render stage class that can render to screen
class VisualizationRenderStage : public AudioRenderStage {
public:
    VisualizationRenderStage(unsigned int frames_per_buffer,
                            unsigned int sample_rate,
                            unsigned int num_channels,
                            const std::string& fragment_shader_source,
                            float history_buffer_size_seconds = 1.0f)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels,
                      fragment_shader_source,
                      true, // use_shader_string flag
                      std::vector<std::string>{
                          "build/shaders/global_settings.glsl",
                          "build/shaders/frag_shader_settings.glsl",
                          "build/shaders/tape_history_settings.glsl"})
    {
        // Create tape history and all its textures/parameters
        m_history2 = std::make_unique<AudioRenderStageHistory2>(
            frames_per_buffer, sample_rate, num_channels, history_buffer_size_seconds);
        
        GLuint active_texture_count = 0;
        m_history2->create_parameters(active_texture_count);
        
        // Add all parameters from history to the render stage
        auto params = m_history2->get_parameters();
        for (auto* param : params) {
            this->add_parameter(param);
        }
    }
    
    // Get access to the history object
    AudioRenderStageHistory2& get_history() { return *m_history2; }
    const AudioRenderStageHistory2& get_history() const { return *m_history2; }
    
    // Override render to render to screen instead of framebuffer
    // Make it public so test code can call it
    void render(const unsigned int time) override {
        // Update the history texture
        m_history2->update_audio_history_texture();
        
        m_time = time;
        
        // First render to the stage framebuffer to capture audio output
        AudioRenderStage::render(time);
        
        // Then render to screen for visualization
        GLint original_fbo = m_framebuffer;
        m_framebuffer = 0; // Screen framebuffer
        
        // Call parent render again - it will now render to screen (framebuffer 0)
        AudioRenderStage::render(time);
        
        // Restore original framebuffer
        m_framebuffer = original_fbo;
    }
    
    void cleanup() {
        unbind();
    }
    
private:
    std::unique_ptr<AudioRenderStageHistory2> m_history2;
};

TEST_CASE("Visualize AudioRenderStageHistory2 texture", "[audio_history2][visualization][gl_test]") {
    constexpr unsigned int FRAMES_PER_BUFFER = 256;
    constexpr unsigned int SAMPLE_RATE = 44100;
    constexpr unsigned int NUM_CHANNELS = 2;
    
    // Set history buffer size to 0.5 seconds (will update 10 times with 5 seconds of data)
    constexpr float HISTORY_BUFFER_SECONDS = 0.5f;
    constexpr float TAPE_DURATION_SECONDS = 5.0f; // 5 seconds of sine data
    
    // Initialize SDL
    REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
    
    // Create visible window using the test framework
    constexpr int WINDOW_WIDTH = 1000;
    constexpr int WINDOW_HEIGHT = 700;
    SDLWindow window(WINDOW_WIDTH, WINDOW_HEIGHT, "History Texture Visualization", true);
    REQUIRE(window.get_window() != nullptr);
    
    // Set up OpenGL viewport
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Create visualization shader source
    const std::string visualization_shader_source = R"(
void main() {
    vec4 color = texture(audio_history_texture, TexCoord);
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);

     // Get texture dimensions to calculate position bar
    ivec2 audio_size = textureSize(audio_history_texture, 0);

    // Get tex coord as int
    ivec2 tex_coord = ivec2(TexCoord.x * float(audio_size.x), TexCoord.y * float(audio_size.y));
    
    // Get channel as int
    int channel = int(TexCoord.y * float(num_channels));

    // Calculate the position of the current position in the audio output texture
    int window_offset = int(TexCoord.x * float(speed_in_samples_per_buffer));
    int position_in_window = tape_position - tape_window_offset_samples + window_offset;

    // Calculate the position of the current position in the audio output texture
    int audio_width = audio_size.x;
    int audio_height = audio_size.y / num_channels / 2; // 2 because we need to store both the audio data and the zeros

    // Calculate the x and y position of the current position in the audio output texture
    int x_position = position_in_window % audio_width;
    int y_row_position = position_in_window / audio_width;

    // Calculate y_position for each channel and check if we're on the correct one
    // Only highlight channel 0 to avoid duplicate lines
    int y_position = ( y_row_position * num_channels + channel ) * 2;

    // Convert the x y into texture coordinates
    vec2 texture_coord = vec2(float(x_position) / float(audio_size.x), float(y_position) / float(audio_size.y));

    output_audio_texture = texture(audio_history_texture, texture_coord);
    debug_audio_texture = stream_audio;
}
)";
    
    // Create visualization render stage - this will initialize the history buffer internally
    VisualizationRenderStage vis_stage(FRAMES_PER_BUFFER, SAMPLE_RATE, NUM_CHANNELS,
                                       visualization_shader_source, HISTORY_BUFFER_SECONDS);
    
    // Initialize the render stage
    REQUIRE(vis_stage.initialize());
    
    // Get access to the history object for setting up tape data
    auto& history = vis_stage.get_history();
    
    // Get the texture dimensions
    unsigned int texture_width = history.m_texture_width;
    unsigned int texture_height = history.m_texture_height;
    unsigned int window_size_samples = history.m_window_size_samples;
    unsigned int texture_rows_per_channel = history.m_texture_rows_per_channel;
    
    std::cout << "Texture dimensions: " << texture_width << " x " << texture_height << std::endl;
    std::cout << "Window size samples: " << window_size_samples << std::endl;
    std::cout << "Texture rows per channel: " << texture_rows_per_channel << std::endl;
    std::cout << "History buffer size: " << HISTORY_BUFFER_SECONDS << " seconds" << std::endl;
    std::cout << "Tape duration: " << TAPE_DURATION_SECONDS << " seconds" << std::endl;
    std::cout << "Expected updates: " << (TAPE_DURATION_SECONDS / HISTORY_BUFFER_SECONDS) << std::endl;
    
    // Generate sine wave data for 5 seconds
    // Create a tape that can hold 5 seconds of data
    unsigned int tape_size_samples = static_cast<unsigned int>(TAPE_DURATION_SECONDS * static_cast<float>(SAMPLE_RATE));
    auto tape = std::make_shared<AudioTape>(FRAMES_PER_BUFFER, SAMPLE_RATE, NUM_CHANNELS, tape_size_samples);
    
    // Generate sine wave: frequency of 440 Hz (A4 note)
    constexpr float FREQUENCY = 440.0f;
    constexpr float TWO_PI = 2.0f * 3.14159265358979323846f;
    
    std::vector<float> sine_wave_data(tape_size_samples * NUM_CHANNELS);
    for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
        for (unsigned int i = 0; i < tape_size_samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float sample = std::sin(TWO_PI * FREQUENCY * t);
            // Add some variation per channel
            sample *= (0.5f + 0.5f * static_cast<float>(ch + 1) / static_cast<float>(NUM_CHANNELS));
            sine_wave_data[ch * tape_size_samples + i] = sample;
        }
    }
    
    // Record the sine wave data to tape in chunks
    for (unsigned int i = 0; i < tape_size_samples; i += FRAMES_PER_BUFFER) {
        unsigned int frames_to_record = std::min(FRAMES_PER_BUFFER, tape_size_samples - i);
        std::vector<float> frame_data(frames_to_record * NUM_CHANNELS);
        
        for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
            for (unsigned int j = 0; j < frames_to_record; ++j) {
                frame_data[ch * frames_to_record + j] = sine_wave_data[ch * tape_size_samples + i + j];
            }
        }
        
        tape->record(frame_data.data(), i);
    }
    
    // Set tape and update texture
    history.set_tape(tape);
    history.set_tape_position(0u);
    history.set_tape_speed(1.0f); // Normal playback speed
    
    // Set up quad geometry
    GLContext context;
    
    // Track updates
    unsigned int previous_offset = history.get_window_offset_samples();
    unsigned int update_count = 0;
    
    std::cout << "Starting visualization loop..." << std::endl;
    
    // Calculate how many frames we need to play through all the tape data
    // At 1.0x speed, we need tape_size_samples / FRAMES_PER_BUFFER frames
    unsigned int frames_to_play = (tape_size_samples + FRAMES_PER_BUFFER - 1) / FRAMES_PER_BUFFER;
    unsigned int total_frames = std::max(600u, frames_to_play + 10); // Play through tape + some extra
    
    // Run for enough frames to see all updates
    for (unsigned int frame = 0; frame < total_frames; ++frame) {
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render to screen (overridden render method renders to framebuffer 0)
        // Note: render() internally updates history texture and sets up parameters
        vis_stage.render(frame);
        
        // Draw the quad to screen
        context.prepare_draw();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        window.swap_buffers();
        
        SDL_Delay(10000); // ~60fps
    }
    
    vis_stage.cleanup();
    
    SDL_Quit();
    
    // Test passes if we got here without crashing
    REQUIRE(true);
}

