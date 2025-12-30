#include <iostream>
#include <algorithm>
#include <cmath>
#include <complex>
#include "graphics_components/equalizer_component.h"
#include "utilities/shader_program.h"
#include "graphics_core/ui_color_palette.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

EqualizerComponent::EqualizerComponent(
    const float x,
    const float y,
    const float width,
    const float height,
    const std::vector<float>& data,
    PositionMode position_mode,
    const bool is_dynamic,
    const std::pair<size_t, size_t>& segments_and_bands,
    const std::pair<float, float>& freq_range,
    const std::pair<float, float>& amp_range,
    const float bar_padding,
    const float segment_padding,
    const std::array<float, 4>& bar_color
) : GraphicsComponent(x, y, width, height, position_mode),
    m_data(&data),
    m_shader_program(nullptr),
    m_vao(0),
    m_vbo(0),
    m_is_dynamic(is_dynamic),
    m_num_segments(segments_and_bands.first),
    m_num_freq_bands(segments_and_bands.second),
    m_freq_min(freq_range.first),
    m_freq_max(freq_range.second),
    m_amp_min_db(amp_range.first),
    m_amp_max_db(amp_range.second),
    m_bar_padding(bar_padding),
    m_segment_padding(segment_padding),
    m_bar_color(bar_color)
{
    // Validate frequency range
    if (m_freq_min <= 0.0f || m_freq_max <= m_freq_min) {
        m_freq_min = 20.0f;
        m_freq_max = 20000.0f;
    }
    
    // Validate amplitude range
    if (m_amp_max_db <= m_amp_min_db) {
        m_amp_min_db = -60.0f;
        m_amp_max_db = 20.0f;
    }
    
    // Validate number of segments
    if (m_num_segments == 0 || m_num_segments > 100) {
        m_num_segments = 8;
    }
    
    // Validate number of frequency bands
    if (m_num_freq_bands == 0 || m_num_freq_bands > 100) {
        m_num_freq_bands = DEFAULT_NUM_FREQ_BANDS;
    }
    
    // Validate padding
    if (m_bar_padding < 0.0f || m_bar_padding >= 1.0f) {
        m_bar_padding = 0.2f;
    }
    // Validate segment padding
    if (m_segment_padding < 0.0f || m_segment_padding >= 1.0f) {
        m_segment_padding = 0.4f;
    }
    // OpenGL resource initialization moved to initialize() method
}

EqualizerComponent::~EqualizerComponent() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
}

float EqualizerComponent::calculate_bar_height(float normalized_value) const {
    // Clamp value between 0 and 1, then scale to component height
    float clamped = std::max(0.0f, std::min(1.0f, normalized_value));
    return clamped * m_height;
}

void EqualizerComponent::get_segment_color(float segment_position, float& r, float& g, float& b, float& a) const {
    // segment_position is 0.0 (bottom) to 1.0 (top)
    // Dual tone: two distinct colors, no gradient
    // Bottom half: darker color, Top half: brighter color
    
    // Get base color from parameter
    float r_val, g_val, b_val, a_val;
    UIColorPalette::get_rgba(m_bar_color, r_val, g_val, b_val, a_val);
    
    // Dual tone: hard switch at midpoint (0.5)
    // Bottom segments (0.0-0.5): full brightness
    // Top segments (0.5-1.0): darker (0.6x brightness)
    if (segment_position < 0.5f) {
        // Bottom half: brighter tone (full color)
        r = r_val;
        g = g_val;
        b = b_val;
    } else {
        // Top half: darker tone
        r = r_val * 0.6f;
        g = g_val * 0.6f;
        b = b_val * 0.6f;
    }
    a = a_val;
}

// Improved FFT implementation with windowing for better accuracy
// Uses larger window size and proper windowing function
void EqualizerComponent::compute_fft(const std::vector<float>& time_data, 
                                      std::vector<float>& magnitude, 
                                      std::vector<float>& frequency) {
    magnitude.clear();
    frequency.clear();
    
    if (time_data.empty()) {
        return;
    }
    
    // Use accumulated history for better frequency resolution
    // Use a window up to FFT_WINDOW_SIZE, taking the most recent samples
    size_t window_size = std::min(FFT_WINDOW_SIZE, time_data.size());
    size_t start_idx = time_data.size() - window_size;  // Use most recent samples
    
    // Pre-calculate Hanning window to reduce spectral leakage
    std::vector<float> window(window_size);
    float window_sum = 0.0f;
    for (size_t n = 0; n < window_size; ++n) {
        // Hanning window: w(n) = 0.5 * (1 - cos(2πn/(N-1)))
        window[n] = 0.5f * (1.0f - std::cos(2.0f * M_PI * n / (window_size - 1)));
        window_sum += window[n];
    }
    // Normalize window to preserve energy
    float window_norm = window_size / window_sum;
    for (size_t n = 0; n < window_size; ++n) {
        window[n] *= window_norm;
    }
    
    // Pre-calculate frequency bins we care about (only need m_num_freq_bands)
    magnitude.reserve(m_num_freq_bands);
    frequency.reserve(m_num_freq_bands);
    
    // Use adjustable frequency range from member variables
    float freq_min = m_freq_min;
    float freq_max = m_freq_max;
    
    // Frequency resolution: sample_rate / window_size
    float freq_resolution = static_cast<float>(SAMPLE_RATE) / window_size;
    
    for (size_t band = 0; band < m_num_freq_bands; ++band) {
        // Calculate center frequency for this band (logarithmic)
        float ratio = (static_cast<float>(band) + 0.5f) / m_num_freq_bands;
        float center_freq = freq_min * std::pow(freq_max / freq_min, ratio);
        
        // Calculate exact FFT bin index (can be fractional for better accuracy)
        float k_exact = center_freq / freq_resolution;
        size_t k = static_cast<size_t>(std::round(k_exact));
        k = std::min(k, window_size / 2 - 1);  // Nyquist limit
        
        // For better accuracy, compute magnitude at exact frequency using Goertzel-like approach
        // or use interpolation between bins. For now, use exact frequency calculation
        float exact_k = center_freq / freq_resolution;
        
        // Compute DFT at exact frequency (more accurate than bin-based)
        float real_sum = 0.0f;
        float imag_sum = 0.0f;
        
        for (size_t n = 0; n < window_size; ++n) {
            // Apply window function
            float windowed_sample = time_data[start_idx + n] * window[n];
            
            // Compute at exact frequency (not just bin frequency)
            float angle = -2.0f * M_PI * exact_k * n / window_size;
            real_sum += windowed_sample * std::cos(angle);
            imag_sum += windowed_sample * std::sin(angle);
        }
        
        // Normalize by window size
        float mag = std::sqrt(real_sum * real_sum + imag_sum * imag_sum) / window_size;
        
        // Apply correction for window function
        mag *= 2.0f;  // Compensate for windowing
        
        magnitude.push_back(mag);
        frequency.push_back(center_freq);
    }
}

float EqualizerComponent::magnitude_to_db(float magnitude) const {
    // Convert magnitude to decibels (optimized for embedded)
    // dB = 20 * log10(magnitude)
    // Fast path for very small values
    if (magnitude < 1e-10f) {
        return -100.0f;  // Very quiet
    }
    // Use natural log and convert (often faster on embedded systems)
    // log10(x) = log(x) / log(10)
    constexpr float LOG10_INV = 0.4342944819f;  // 1 / log(10)
    return 20.0f * std::log(magnitude) * LOG10_INV;
}

void EqualizerComponent::map_frequencies_to_bands(const std::vector<float>& frequencies,
                                                   const std::vector<float>& magnitudes,
                                                   std::vector<float>& band_magnitudes) {
    // Since compute_fft already calculates band center frequencies,
    // we can directly use the magnitudes (one per band)
    band_magnitudes = magnitudes;
    
    // Ensure we have the right number of bands
    if (band_magnitudes.size() != m_num_freq_bands) {
        band_magnitudes.resize(m_num_freq_bands, 0.0f);
    }
}

bool EqualizerComponent::initialize() {
    // Initialize shader program
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec4 a_color;
        out vec4 v_color;
        
        void main() {
            // Position is already in normalized device coordinates
            gl_Position = vec4(a_position, 0.0, 1.0);
            v_color = a_color;
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        in vec4 v_color;
        out vec4 frag_color;
        
        void main() {
            frag_color = v_color;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for EqualizerComponent" << std::endl;
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    // Initialize with current data
    set_data(*m_data);

    GraphicsComponent::initialize();
    
    return true;
}

void EqualizerComponent::set_data(const std::vector<float>& data) {
    m_data = &data;

    // Throttle updates to reduce CPU usage (only update every N frames)
    m_render_frame_counter++;
    if (m_render_frame_counter % UPDATE_THROTTLE != 0) {
        return;  // Skip this update
    }

    // Only update buffer if OpenGL resources are initialized
    if (m_vbo != 0 && m_data != nullptr && !m_data->empty()) {
        // Accumulate audio data into history buffer
        // Append new data to history
        m_audio_history.insert(m_audio_history.end(), m_data->begin(), m_data->end());
        
        // Limit history size to prevent unbounded memory growth
        if (m_audio_history.size() > MAX_HISTORY_SIZE) {
            // Keep only the most recent samples (sliding window)
            size_t excess = m_audio_history.size() - MAX_HISTORY_SIZE;
            m_audio_history.erase(m_audio_history.begin(), m_audio_history.begin() + excess);
        }
        
        
        // Calculate vertices in component-local coordinates (-1 to 1)
        // After begin_local_rendering(), coordinates are relative to component viewport
        // So we use -1 to 1 where (-1, -1) is bottom-left and (1, 1) is top-right
        std::vector<float> vertices;
        
        // Perform frequency domain analysis on accumulated history
        std::vector<float> frequency_magnitudes;
        std::vector<float> frequencies;
        compute_fft(m_audio_history, frequency_magnitudes, frequencies);
        
        // Map frequencies to frequency bands
        std::vector<float> band_magnitudes;
        map_frequencies_to_bands(frequencies, frequency_magnitudes, band_magnitudes);
        
        const size_t num_bars = band_magnitudes.size();
        if (num_bars == 0) {
            m_vertex_count = 0;
            return;
        }
        
        // Convert magnitudes to dB scale and find actual range
        std::vector<float> band_db_values;
        band_db_values.reserve(num_bars);
        
        float max_db = -100.0f;
        float min_db = 0.0f;
        float max_magnitude = 0.0f;
        
        for (float mag : band_magnitudes) {
            max_magnitude = std::max(max_magnitude, mag);
            float db = magnitude_to_db(mag);
            band_db_values.push_back(db);
            max_db = std::max(max_db, db);
            if (mag > 1e-6f) {  // Only consider non-zero magnitudes
                min_db = (min_db == 0.0f) ? db : std::min(min_db, db);
            }
        }
        
        // More sensitive normalization - use magnitude directly for better sensitivity
        // Instead of dB which compresses quiet signals, use logarithmic scaling of magnitude
        // This makes the equalizer more responsive to quieter signals
        
        // Find the range of magnitudes (not dB) for more sensitive display
        float min_magnitude = max_magnitude;
        for (float mag : band_magnitudes) {
            if (mag > 1e-8f) {  // Only consider non-zero
                min_magnitude = std::min(min_magnitude, mag);
            }
        }
        
        // Use logarithmic scaling of magnitude for better sensitivity
        // This makes quiet signals more visible
        std::vector<float> normalized_values;
        normalized_values.reserve(num_bars);
        
        // Calculate normalized values using logarithmic magnitude scaling
        float magnitude_range = max_magnitude - min_magnitude;
        if (magnitude_range < 1e-6f) {
            magnitude_range = max_magnitude;  // Use max as range if all similar
        }
        
        // Use a combination: magnitude-based for sensitivity, but still show dB for reference
        // Scale magnitudes logarithmically for better visualization
        float log_max = std::log10(std::max(max_magnitude, 1e-6f));
        float log_min = std::log10(std::max(min_magnitude, 1e-8f));
        float log_range = log_max - log_min;
        
        // If range is too small, use a fixed range
        if (log_range < 0.5f) {
            log_range = 2.0f;  // 2 orders of magnitude range
            log_min = log_max - log_range;
        }
        
        // Normalize using logarithmic scale for better sensitivity
        for (size_t i = 0; i < num_bars; ++i) {
            float mag = band_magnitudes[i];
            if (mag > 1e-8f) {
                float log_mag = std::log10(mag);
                float normalized = (log_mag - log_min) / log_range;
                normalized = std::max(0.0f, std::min(1.0f, normalized));
                // Boost quiet signals - apply a curve to make them more visible
                normalized = std::pow(normalized, 0.7f);  // Gamma correction for better visibility
                normalized_values.push_back(normalized);
            } else {
                normalized_values.push_back(0.0f);
            }
        }
        
        // Use adjustable amplitude range from member variables
        // Normalize dB values using the configured amplitude range
        float db_min = m_amp_min_db;
        float db_max = m_amp_max_db;
        float db_range = db_max - db_min;
        
        // Ensure we have a reasonable range
        if (db_range < 10.0f) {
            db_range = 40.0f;  // Use minimum 40dB range
            db_min = db_max - db_range;
        }
        
        // In component-local coordinates, width and height are both 2.0 (from -1 to 1)
        const float slot_width = 2.0f / num_bars;
        // Apply padding: subtract padding from bar width
        // m_bar_padding is fraction of slot_width to be empty space
        const float actual_bar_width = slot_width * (1.0f - m_bar_padding);
        const float margin = (slot_width - actual_bar_width) / 2.0f;
        
        // Calculate segment height accounting for padding
        // Total height is 2.0, divided by num_segments, with padding between segments
        const float total_segment_space = 2.0f / m_num_segments;
        const float segment_padding_height = total_segment_space * m_segment_padding;
        const float actual_segment_height = total_segment_space * (1.0f - m_segment_padding);
        
        for (size_t bar_idx = 0; bar_idx < num_bars; ++bar_idx) {
            // Convert magnitude to dB for amplitude range normalization
            float mag = band_magnitudes[bar_idx];
            float db_value = magnitude_to_db(mag);
            
            // Normalize using the adjustable amplitude range
            float normalized_value = (db_value - db_min) / db_range;
            normalized_value = std::max(0.0f, std::min(1.0f, normalized_value));
            
            // Apply gamma correction for better visibility of quiet signals
            normalized_value = std::pow(normalized_value, 0.7f);
            
            // Ensure minimum visibility - if there's any signal, show at least a small bar
            if (mag > 1e-8f && normalized_value < 0.05f) {
                normalized_value = 0.05f + normalized_value * 0.2f;  // Show at least 5-7% height
            }
            
            // Scale to use more of the available height (optional boost)
            normalized_value = normalized_value * 0.95f + 0.05f;  // Scale to [0.05, 1.0] range
            normalized_value = std::min(1.0f, normalized_value);
            
            // Calculate bar height in component-local coordinates (0 to 2.0)
            float bar_height = normalized_value * 2.0f;
            
            // Calculate how many segments should be filled
            // bar_height is in [0, 2.0] range, account for segment spacing
            size_t filled_segments = 0;
            if (bar_height > 0.001f) {
                // Calculate how many segments fit, accounting for padding
                float current_height = 0.0f;
                for (size_t i = 0; i < m_num_segments; ++i) {
                    if (current_height + actual_segment_height <= bar_height) {
                        filled_segments++;
                        current_height += actual_segment_height + segment_padding_height;
                    } else {
                        break;
                    }
                }
                filled_segments = std::min(filled_segments, m_num_segments);
            }
            
            // Only show segments if there's actual signal (don't force minimum)
            // This prevents showing "ghost" bars when there's no audio
            
            
            // Position of this bar in component-local coordinates
            // X goes from -1 (left) to 1 (right)
            // Center the bar within its slot
            float bar_left_x = -1.0f + bar_idx * slot_width + margin;
            float bar_right_x = bar_left_x + actual_bar_width;
            
            // Draw segments from bottom to top
            // Y goes from -1 (bottom) to 1 (top)
            // Segments should stack: seg 0 at bottom, seg 1 above it, etc.
            // Add spacing between segments
            for (size_t seg_idx = 0; seg_idx < filled_segments; ++seg_idx) {
                // Calculate Y positions - segments stack from bottom (-1) upward with padding
                // Each segment has padding after it (except possibly the last one)
                float base_y = -1.0f + seg_idx * (actual_segment_height + segment_padding_height);
                float seg_bottom_y = base_y;
                float seg_top_y = seg_bottom_y + actual_segment_height;
                
                // Ensure segments don't go above the bar height
                // But preserve the gap - don't draw if there's not enough space
                float max_y = -1.0f + bar_height;
                if (seg_bottom_y >= max_y) {
                    // This segment is completely above the bar height, skip it
                    continue;
                }
                if (seg_top_y > max_y) {
                    // Partial segment at the top - only draw if there's enough space
                    if (max_y - seg_bottom_y >= actual_segment_height * 0.3f) {
                        seg_top_y = max_y;
                    } else {
                        // Not enough space, skip this segment to preserve gap
                        continue;
                    }
                }
                
                // Calculate color based on segment position in bar (0.0 = bottom, 1.0 = top)
                // Use position relative to total bar height, not just filled segments
                float segment_position = static_cast<float>(seg_idx) / m_num_segments;
                float r, g, b, a;
                get_segment_color(segment_position, r, g, b, a);
                
                // Create rectangle for this segment (two triangles)
                // Triangle 1: bottom-left, top-left, top-right
                vertices.push_back(bar_left_x);      // x
                vertices.push_back(seg_bottom_y);    // y (bottom)
                vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
                
                vertices.push_back(bar_left_x);      // x
                vertices.push_back(seg_top_y);      // y (top)
                vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
                
                vertices.push_back(bar_right_x);     // x
                vertices.push_back(seg_top_y);      // y (top)
                vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
                
                // Triangle 2: bottom-left, top-right, bottom-right
                vertices.push_back(bar_left_x);      // x
                vertices.push_back(seg_bottom_y);    // y (bottom)
                vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
                
                vertices.push_back(bar_right_x);     // x
                vertices.push_back(seg_top_y);      // y (top)
                vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
                
                vertices.push_back(bar_right_x);     // x
                vertices.push_back(seg_bottom_y);   // y (bottom)
                vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(a);
            }
        }
        
        m_vertex_count = vertices.size() / 6;  // Store number of triangles
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        if (m_is_dynamic) {
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        } else {
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        }
    } else {
        m_vertex_count = 0;
    }
}

void EqualizerComponent::render_content() {
    if (!m_data || m_data->empty() || !m_shader_program || m_vao == 0) {
        return;
    }

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Use the shader program
    glUseProgram(m_shader_program->get_program());

    // Bind and update vertex data if needed
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    if (m_is_dynamic) {
        // Recalculate vertices with updated data (throttled internally)
        set_data(*m_data);
    }

    // Set up vertex attributes
    // Position: 2 floats, Color: 4 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));

    // Draw triangles using the stored vertex count
    if (m_vertex_count > 0) {
        glDrawArrays(GL_TRIANGLES, 0, m_vertex_count * 6);
    }

    // Clean up
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void EqualizerComponent::set_bar_color(float r, float g, float b, float a) {
    m_bar_color[0] = r;
    m_bar_color[1] = g;
    m_bar_color[2] = b;
    m_bar_color[3] = a;
    // Trigger recalculation on next render
    if (m_data && !m_data->empty()) {
        set_data(*m_data);
    }
}

void EqualizerComponent::set_bar_color(const std::array<float, 4>& color) {
    set_bar_color(color[0], color[1], color[2], color[3]);
}

void EqualizerComponent::set_num_freq_bands(size_t num_bands) {
    if (num_bands == 0 || num_bands > 100) {
        return;  // Invalid, ignore
    }
    if (m_num_freq_bands != num_bands) {
        m_num_freq_bands = num_bands;
        // Trigger recalculation on next render
        if (m_data && !m_data->empty()) {
            set_data(*m_data);
        }
    }
}

void EqualizerComponent::set_num_segments(size_t num_segments) {
    if (num_segments == 0 || num_segments > 100) {
        return;  // Invalid, ignore
    }
    if (m_num_segments != num_segments) {
        m_num_segments = num_segments;
        // Trigger recalculation on next render
        if (m_data && !m_data->empty()) {
            set_data(*m_data);
        }
    }
}

void EqualizerComponent::set_freq_range(float freq_min, float freq_max) {
    if (freq_min <= 0.0f || freq_max <= freq_min) {
        return;  // Invalid, ignore
    }
    if (m_freq_min != freq_min || m_freq_max != freq_max) {
        m_freq_min = freq_min;
        m_freq_max = freq_max;
        // Trigger recalculation on next render
        if (m_data && !m_data->empty()) {
            set_data(*m_data);
        }
    }
}

void EqualizerComponent::set_freq_range(const std::pair<float, float>& freq_range) {
    set_freq_range(freq_range.first, freq_range.second);
}

void EqualizerComponent::set_amp_range(float amp_min_db, float amp_max_db) {
    if (amp_max_db <= amp_min_db) {
        return;  // Invalid, ignore
    }
    if (m_amp_min_db != amp_min_db || m_amp_max_db != amp_max_db) {
        m_amp_min_db = amp_min_db;
        m_amp_max_db = amp_max_db;
        // Trigger recalculation on next render
        if (m_data && !m_data->empty()) {
            set_data(*m_data);
        }
    }
}

void EqualizerComponent::set_amp_range(const std::pair<float, float>& amp_range) {
    set_amp_range(amp_range.first, amp_range.second);
}

void EqualizerComponent::set_bar_padding(float padding) {
    if (padding < 0.0f || padding >= 1.0f) {
        return;  // Invalid, ignore
    }
    if (m_bar_padding != padding) {
        m_bar_padding = padding;
        // Trigger recalculation on next render
        if (m_data && !m_data->empty()) {
            set_data(*m_data);
        }
    }
}

void EqualizerComponent::set_segment_padding(float padding) {
    if (padding < 0.0f || padding >= 1.0f) {
        return;  // Invalid, ignore
    }
    if (m_segment_padding != padding) {
        m_segment_padding = padding;
        // Trigger recalculation on next render
        if (m_data && !m_data->empty()) {
            set_data(*m_data);
        }
    }
}

