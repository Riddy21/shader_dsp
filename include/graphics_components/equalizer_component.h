#ifndef EQUALIZER_COMPONENT_H
#define EQUALIZER_COMPONENT_H

#include <memory>
#include <vector>
#include <utility>
#include <array>
#include <SDL2/SDL.h>

#include "utilities/shader_program.h"
#include "engine/event_handler.h"
#include "graphics_core/graphics_component.h"
#include "graphics_core/ui_color_palette.h"

class EqualizerComponent : public GraphicsComponent {
public:
    EqualizerComponent(
        const float x, const float y, const float width, const float height,
        const std::vector<float>& data,
        PositionMode position_mode = PositionMode::TOP_LEFT,
        const bool is_dynamic = true,
        const std::pair<size_t, size_t>& segments_and_bands = {6, 8},  // {num_segments, num_freq_bands}
        const std::pair<float, float>& freq_range = {50.0f, 10000.0f},  // {freq_min, freq_max} in Hz
        const std::pair<float, float>& amp_range = {-60.0f, 20.0f},      // {amp_min_db, amp_max_db} in dB
        const float bar_padding = 0.2f,                                  // Padding between bars (0.0-1.0 relative to slot width)
        const float segment_padding = 0.4f,                              // Padding between segments vertically (0.0-1.0 relative to segment height)
        const std::array<float, 4>& bar_color = UIColorPalette::PRIMARY_YELLOW // Color of the bars
    );
    ~EqualizerComponent() override;

    void set_data(const std::vector<float>& data);
    
    // Setter functions for equalizer parameters
    void set_bar_color(float r, float g, float b, float a);
    void set_bar_color(const std::array<float, 4>& color);
    void set_num_freq_bands(size_t num_bands);
    void set_num_segments(size_t num_segments);
    void set_freq_range(float freq_min, float freq_max);
    void set_freq_range(const std::pair<float, float>& freq_range);
    void set_amp_range(float amp_min_db, float amp_max_db);
    void set_amp_range(const std::pair<float, float>& amp_range);
    void set_bar_padding(float padding);
    void set_segment_padding(float padding);
    
protected:
    bool initialize() override;
    void render_content() override;

private:
    bool m_is_dynamic = true;
    const std::vector<float>* m_data;
    size_t m_num_segments;  // Number of segments per bar
    size_t m_num_freq_bands;  // Number of frequency bands
    
    // Adjustable frequency range
    float m_freq_min;  // Minimum frequency in Hz
    float m_freq_max;  // Maximum frequency in Hz
    
    // Adjustable amplitude range
    float m_amp_min_db;  // Minimum amplitude in dB for normalization
    float m_amp_max_db;  // Maximum amplitude in dB for normalization
    
    // Appearance
    float m_bar_padding;  // Padding between bars
    float m_segment_padding;  // Padding between segments vertically
    std::array<float, 4> m_bar_color;  // Color of the bars
    
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
    size_t m_vertex_count = 0;  // Store number of triangles for rendering
    
    // Render throttling - only update every N frames
    size_t m_render_frame_counter = 0;
    
    // Audio history buffer for accumulating samples over time
    std::vector<float> m_audio_history;
    static constexpr size_t MAX_HISTORY_SIZE = 8192;  // Maximum history size (~186ms @ 44.1kHz)
                                                       // Larger history for better frequency resolution
    
    // Helper method to calculate bar height from normalized value
    float calculate_bar_height(float normalized_value) const;
    
    // Helper method to get color for a segment based on its position in the bar
    void get_segment_color(float segment_position, float& r, float& g, float& b, float& a) const;
    
    // Frequency domain analysis methods
    void compute_fft(const std::vector<float>& time_data, std::vector<float>& magnitude, std::vector<float>& frequency);
    float magnitude_to_db(float magnitude) const;
    void map_frequencies_to_bands(const std::vector<float>& frequencies, 
                                   const std::vector<float>& magnitudes,
                                   std::vector<float>& band_magnitudes);
    
    // Low-CPU frequency analysis constants (optimized for embedded systems)
    static constexpr unsigned int SAMPLE_RATE = 44100;  // Standard audio sample rate
    static constexpr size_t DEFAULT_NUM_FREQ_BANDS = 10;  // Default number of frequency bands
    static constexpr size_t FFT_WINDOW_SIZE = 2048;  // FFT window size (2048 samples = ~46.4ms @ 44.1kHz)
                                                      // Larger window = better frequency resolution
                                                      // Frequency resolution = sample_rate / window_size = ~21.5 Hz
    static constexpr size_t UPDATE_THROTTLE = 5;  // Update every N frames (5 = ~80% CPU reduction)
                                                    // Only recalculate FFT and vertices every 5 frames
};

#endif // EQUALIZER_COMPONENT_H

