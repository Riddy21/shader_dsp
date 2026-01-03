#include "graphics_components/track_display_component.h"
#include "utilities/shader_program.h"
#include "graphics_core/ui_color_palette.h"
#include "audio_core/audio_tape.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
// TrackMeasureComponent Implementation
// ============================================================================

TrackMeasureComponent::TrackMeasureComponent(
    float x, float y, float width, float height,
    PositionMode position_mode,
    float bpm,
    float zoom,
    float position_seconds
) : GraphicsComponent(x, y, width, height, position_mode),
    m_bpm(bpm),
    m_zoom(zoom),
    m_position_seconds(position_seconds),
    m_shader_program(nullptr),
    m_vao(0),
    m_vbo(0),
    m_vertex_count(0)
{
    // Ensure reasonable defaults
    if (m_bpm <= 0.0f) m_bpm = 120.0f; // Default to 120 BPM
    if (m_zoom <= 0.0f) m_zoom = 1.0f;
    // Default position is 0 (center at time 0)
    if (m_position_seconds < 0.0f) m_position_seconds = 0.0f;
}

TrackMeasureComponent::~TrackMeasureComponent() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
}

size_t TrackMeasureComponent::calculate_num_ticks() const {
    // Calculate visible duration
    float visible_duration = MAX_TIMELINE_DURATION_SECONDS / m_zoom;
    
    // Calculate beat duration in seconds (60 seconds per minute / beats per minute)
    float beat_duration = 60.0f / m_bpm;
    
    // Calculate how many beats fit in the visible duration
    float num_beats = visible_duration / beat_duration;
    
    // Return number of ticks (beats) to show, add 1 to include the end tick
    // Cap at reasonable maximum to avoid performance issues
    size_t num_ticks = static_cast<size_t>(num_beats) + 1;
    return std::min(num_ticks, static_cast<size_t>(1000)); // Max 1000 ticks
}

bool TrackMeasureComponent::initialize() {
    // Shader for drawing vertical tick lines using instanced rendering
    // The shader calculates tick positions based on BPM, zoom, and position
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;  // x ignored, y used
        
        uniform float uNumTicks;  // Number of ticks (beats) to render
        uniform float uZoom;  // Zoom level (1.0 = full view)
        uniform float uPosition;  // Current scroll position in seconds
        uniform float uMaxDuration;  // Maximum timeline duration (600 seconds = 10 minutes)
        uniform float uBPM;  // Beats per minute
        
        void main() {
            // Calculate visible time range based on zoom and position
            // Position represents the CENTER of the viewport (position 0 = center at time 0)
            float visible_duration = uMaxDuration / uZoom;
            float center_time = uPosition; // Position directly represents center time
            float start_time = center_time - visible_duration * 0.5;
            float end_time = center_time + visible_duration * 0.5;
            
            // Calculate beat duration in seconds
            float beat_duration = 60.0 / uBPM;
            
            // Calculate the first beat time that's >= start_time
            // We use floor even for negative start_time to ensure we get the beat before or at start_time
            float first_beat_time = floor(start_time / beat_duration) * beat_duration;
            
            // Calculate tick time for this instance (beat time)
            float tick_time = first_beat_time + float(gl_InstanceID) * beat_duration;
            
            // Only render if tick is within visible range
            if (tick_time < start_time || tick_time > end_time) {
                // Move off-screen
                gl_Position = vec4(2.0, 0.0, 0.0, 1.0);
                return;
            }
            
            // Only render ticks within the valid timeline range (0 to max)
            if (tick_time < 0.0 || tick_time > uMaxDuration) {
                gl_Position = vec4(2.0, 0.0, 0.0, 1.0);
                return;
            }
            
            // Map time to x position in normalized device coordinates [-1.0, 1.0]
            // Map from visible range [start_time, end_time] to screen space [-1.0, 1.0]
            float normalized_time = (tick_time - start_time) / visible_duration;
            float x_pos = mix(-0.995, 0.995, normalized_time);
            
            // Use the y coordinate from the vertex, x calculated from time
            gl_Position = vec4(x_pos, aPos.y, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for TrackMeasureComponent" << std::endl;
        return false;
    }

    // Create VAO and VBO for a single vertical line segment
    // We only need 2 vertices (bottom and top), and use instancing for multiple ticks
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    // Create a single vertical line segment (x ignored, calculated in shader)
    // In component-local coordinates: y=-1 is bottom, y=1 is top
    float vertices[] = {
        0.0f, -1.0f,  // x (ignored), y (bottom)
        0.0f,  1.0f   // x (ignored), y (top)
    };

    // Set up VAO
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Set up vertex attributes (vec2: x and y, but x will be recalculated in shader)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // We always have 2 vertices (one line segment)
    m_vertex_count = 2;

    GraphicsComponent::initialize();
    return true;
}

void TrackMeasureComponent::set_zoom(float zoom) {
    if (zoom > 0.0f) {
        m_zoom = zoom;
    }
}

void TrackMeasureComponent::set_position(float position_seconds) {
    // Position represents the center of the viewport (position 0 = center at time 0)
    // Clamp to valid range based on current zoom level to keep viewport within bounds
    if (position_seconds >= 0.0f) {
        float visible_duration = MAX_TIMELINE_DURATION_SECONDS / m_zoom;
        float max_center = MAX_TIMELINE_DURATION_SECONDS - visible_duration * 0.5f;
        
        // Clamp center position to keep viewport within bounds
        // Allow position 0 (which will show from 0 to visible_duration, centered at 0)
        m_position_seconds = std::max(0.0f, std::min(position_seconds, max_center));
    }
}

void TrackMeasureComponent::set_bpm(float bpm) {
    if (bpm > 0.0f) {
        m_bpm = bpm;
    }
}

void TrackMeasureComponent::render_content() {
    if (!m_shader_program || m_vao == 0) return;

    // Calculate number of ticks based on visible duration and BPM
    size_t num_ticks = calculate_num_ticks();
    if (num_ticks == 0) return;

    glUseProgram(m_shader_program->get_program());

    // Set uniforms - use primary yellow color
    GLuint program = m_shader_program->get_program();
    glUniform4f(glGetUniformLocation(program, "uColor"), 
                UIColorPalette::PRIMARY_YELLOW[0],
                UIColorPalette::PRIMARY_YELLOW[1],
                UIColorPalette::PRIMARY_YELLOW[2],
                UIColorPalette::PRIMARY_YELLOW[3]);
    glUniform1f(glGetUniformLocation(program, "uNumTicks"), static_cast<float>(num_ticks));
    glUniform1f(glGetUniformLocation(program, "uZoom"), m_zoom);
    glUniform1f(glGetUniformLocation(program, "uPosition"), m_position_seconds);
    glUniform1f(glGetUniformLocation(program, "uMaxDuration"), MAX_TIMELINE_DURATION_SECONDS);
    glUniform1f(glGetUniformLocation(program, "uBPM"), m_bpm);

    // Draw tick lines using instanced rendering
    // We have 2 vertices (one line segment) and draw num_ticks instances
    glBindVertexArray(m_vao);
    glLineWidth(2.0f); // Make tick marks thicker
    glDrawArraysInstanced(GL_LINES, 0, 2, num_ticks);
    glBindVertexArray(0);

    glUseProgram(0);
}

// ============================================================================
// TrackRowComponent Implementation
// ============================================================================

TrackRowComponent::TrackRowComponent(float x, float y, float width, float height,
                                     PositionMode position_mode,
                                     AudioTape* tape,
                                     float total_timeline_duration_seconds,
                                     float zoom,
                                     float position_seconds)
    : GraphicsComponent(x, y, width, height, position_mode),
      m_shader_program(nullptr),
      m_vao(0),
      m_vbo(0),
      m_amplitude_texture(0),
      m_tape(tape),
      m_total_timeline_duration_seconds(total_timeline_duration_seconds),
      m_audio_start_offset_seconds(0.0f),
      m_zoom(zoom),
      m_position_seconds(position_seconds),
      m_selected(false),
      m_amplitude_texture_dirty(true),
      m_last_tape_size(0),
      m_last_record_position(0),
      m_update_frame_counter(0)
{
    // Ensure reasonable defaults
    if (m_zoom <= 0.0f) m_zoom = 1.0f;
    // Default position is 0 (center at time 0)
    if (m_position_seconds < 0.0f) m_position_seconds = 0.0f;
    
    // Initialize tracking values if tape is available
    if (m_tape) {
        m_last_tape_size = m_tape->size();
        m_last_record_position = m_tape->get_current_record_position();
    }
}

TrackRowComponent::~TrackRowComponent() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_amplitude_texture != 0) {
        glDeleteTextures(1, &m_amplitude_texture);
    }
}

bool TrackRowComponent::initialize() {
    // Shader for drawing audio visualization with amplitude-based coloring
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uAmplitudeTexture;
        uniform vec2 uAudioRegion; // x = start normalized, y = end normalized (in timeline time)
        uniform bool uSelected;
        uniform vec4 uYellowColor;
        uniform vec4 uOrangeColor;
        uniform vec4 uGreyColor;
        uniform float uThickness;
        uniform float uZoom;  // Zoom level (1.0 = full view)
        uniform float uPosition;  // Current scroll position in seconds
        uniform float uMaxDuration;  // Maximum timeline duration (600 seconds = 10 minutes)
        
        void main() {
            // Calculate visible time range based on zoom and position
            // Position represents the CENTER of the viewport (position 0 = center at time 0)
            float visible_duration = uMaxDuration / uZoom;
            float center_time = uPosition; // Position directly represents center time
            float visible_start_time = center_time - visible_duration * 0.5;
            float visible_end_time = center_time + visible_duration * 0.5;
            
            // Map screen x coordinate (TexCoord.x: 0.0 to 1.0) to timeline time
            // No clamping of start/end times here - we want to preserve linear mapping
            // screen_time can be negative (if we are scrolling past 0) or > max_duration
            float screen_time = visible_start_time + TexCoord.x * visible_duration;
            
            // Only render content for valid timeline range
            if (screen_time < 0.0 || screen_time > uMaxDuration) {
                discard;
            }
            
            // Check if we're within the audio region horizontally (in timeline time)
            if (screen_time < uAudioRegion.x || screen_time > uAudioRegion.y) {
                discard; // Outside audio region
            }
            
            // Map timeline time to audio region for amplitude sampling
            float audio_t = (screen_time - uAudioRegion.x) / (uAudioRegion.y - uAudioRegion.x);
            audio_t = clamp(audio_t, 0.0, 1.0); // Ensure valid range for texture sampling
            
            // Sample amplitude from texture (1D texture, use x coordinate)
            float amplitude = texture(uAmplitudeTexture, vec2(audio_t, 0.0)).r;
            
            // Apply power curve to make amplitude differences more obvious
            // Square root makes low values more visible, square makes high values stand out
            float amplitude_enhanced = pow(amplitude, 0.5); // Square root for better contrast
            
            // Calculate color based on selection and amplitude
            vec4 color;
            if (uSelected) {
                // Selected: fade from yellow (low amplitude) to bright orange/red (high amplitude)
                // Use a wider color range for more obvious difference
                vec4 low_color = uYellowColor;
                vec4 high_color = vec4(1.0, 0.3, 0.0, 1.0); // Bright orange-red for high amplitude
                color = mix(low_color, high_color, amplitude_enhanced);
            } else {
                // Not selected: grey-black based on amplitude with wider range
                float grey_intensity = 0.2 + amplitude_enhanced * 0.4; // Range from 0.2 to 0.6
                color = vec4(grey_intensity, grey_intensity, grey_intensity, 0.8);
            }
            
            // Apply thickness check (only render within thickness bounds)
            // TexCoord.y goes from 0 (bottom) to 1 (top), center is at 0.5
            // uThickness is in NDC space (0.0 to 1.0), so we check if we're within thickness/2 of center
            float y_dist_from_center = abs(TexCoord.y - 0.5);
            if (y_dist_from_center > uThickness * 0.5) {
                discard;
            }
            
            FragColor = color;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for TrackRowComponent" << std::endl;
        return false;
    }

    // Create a full-screen quad with texture coordinates
    // In component-local coordinates: (-1, -1) is bottom-left, (1, 1) is top-right
    // Texture coordinates: (0, 0) to (1, 1)
    float vertices[] = {
        // x, y, u, v
        -1.0f, -1.0f, 0.0f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f, 1.0f,  // top left
         1.0f,  1.0f, 1.0f, 1.0f,  // top right
        
        -1.0f, -1.0f, 0.0f, 0.0f,  // bottom left
         1.0f,  1.0f, 1.0f, 1.0f,  // top right
         1.0f, -1.0f, 1.0f, 0.0f   // bottom right
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenTextures(1, &m_amplitude_texture);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Initialize amplitude texture (1D texture, will be updated when audio data changes)
    glBindTexture(GL_TEXTURE_2D, m_amplitude_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Create empty texture initially (will be filled when audio is available)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 256, 1, 0, GL_RED, GL_FLOAT, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GraphicsComponent::initialize();
    return true;
}

bool TrackRowComponent::should_update_texture() const {
    if (!m_tape) {
        return false;
    }
    
    // Always update if explicitly marked dirty
    if (m_amplitude_texture_dirty) {
        return true;
    }
    
    // Check if tape data has changed
    unsigned int current_size = m_tape->size();
    unsigned int current_record_pos = m_tape->get_current_record_position();
    
    bool tape_changed = (current_size != m_last_tape_size) || 
                        (current_record_pos != m_last_record_position);
    
    // Update if tape changed and enough frames have passed (throttling)
    if (tape_changed && m_update_frame_counter >= UPDATE_THROTTLE_FRAMES) {
        return true;
    }
    
    return false;
}

void TrackRowComponent::update_amplitude_texture() {
    if (!m_tape || m_total_timeline_duration_seconds <= 0.0f) {
        return;
    }
    
    float audio_duration = m_tape->size_in_seconds();
    if (audio_duration <= 0.0f) {
        return;
    }
    
    // Sample audio amplitude at regular intervals
    const int texture_width = 256; // Resolution of amplitude texture
    std::vector<float> amplitudes;
    amplitudes.reserve(texture_width);
    
    unsigned int sample_rate = m_tape->sample_rate();
    unsigned int samples_per_segment = sample_rate / 100; // ~10ms chunks
    
    // Sample audio amplitude at different points
    for (int i = 0; i < texture_width; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(texture_width - 1);
        float sample_time = m_audio_start_offset_seconds + t * audio_duration;
        unsigned int sample_offset = static_cast<unsigned int>(sample_time * sample_rate);
        
        auto audio_chunk = m_tape->playback(samples_per_segment, sample_offset, false);
        
        // Calculate RMS amplitude for this segment
        float rms = 0.0f;
        if (!audio_chunk.empty() && m_tape->num_channels() > 0) {
            unsigned int samples_per_channel = static_cast<unsigned int>(audio_chunk.size()) / m_tape->num_channels();
            for (unsigned int ch = 0; ch < m_tape->num_channels(); ++ch) {
                for (unsigned int s = 0; s < samples_per_channel; ++s) {
                    float sample = audio_chunk[ch * samples_per_channel + s];
                    rms += sample * sample;
                }
            }
            rms = std::sqrt(rms / (samples_per_channel * m_tape->num_channels()));
        }
        
        // Normalize amplitude with more aggressive scaling for better visibility
        // Use sqrt to compress high values and make differences more visible
        float normalized_amplitude = std::min(1.0f, rms * 4.0f); // Scale up more aggressively
        amplitudes.push_back(normalized_amplitude);
    }
    
    // Upload amplitude data to texture
    glBindTexture(GL_TEXTURE_2D, m_amplitude_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texture_width, 1, 0, GL_RED, GL_FLOAT, amplitudes.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Update tracking values
    m_last_tape_size = m_tape->size();
    m_last_record_position = m_tape->get_current_record_position();
    m_amplitude_texture_dirty = false;
    m_update_frame_counter = 0; // Reset counter after update
}

void TrackRowComponent::render_content() {
    if (!m_shader_program || m_vao == 0) return;

    // Increment frame counter for throttling
    m_update_frame_counter++;
    
    // Update amplitude texture if needed (checks for changes and throttles)
    if (should_update_texture()) {
        update_amplitude_texture();
    }

    // If we don't have audio data, don't render anything
    if (!m_tape || m_total_timeline_duration_seconds <= 0.0f) {
        return;
    }
    
    float audio_duration = m_tape->size_in_seconds();
    if (audio_duration <= 0.0f) {
        return;
    }

    // Calculate audio region in timeline time (seconds)
    float audio_start_time = m_audio_start_offset_seconds;
    float audio_end_time = m_audio_start_offset_seconds + audio_duration;
    
    // Clamp to valid timeline range [0, MAX_TIMELINE_DURATION_SECONDS]
    audio_start_time = std::max(0.0f, std::min(audio_start_time, MAX_TIMELINE_DURATION_SECONDS));
    audio_end_time = std::max(0.0f, std::min(audio_end_time, MAX_TIMELINE_DURATION_SECONDS));
    
    if (audio_end_time <= audio_start_time) {
        return; // No valid audio region
    }

    glUseProgram(m_shader_program->get_program());

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint program = m_shader_program->get_program();
    
    // Set uniforms - audio region is now in timeline time (seconds)
    glUniform2f(glGetUniformLocation(program, "uAudioRegion"), audio_start_time, audio_end_time);
    glUniform1f(glGetUniformLocation(program, "uZoom"), m_zoom);
    glUniform1f(glGetUniformLocation(program, "uPosition"), m_position_seconds);
    glUniform1f(glGetUniformLocation(program, "uMaxDuration"), MAX_TIMELINE_DURATION_SECONDS);
    glUniform1i(glGetUniformLocation(program, "uSelected"), m_selected ? 1 : 0);
    glUniform4f(glGetUniformLocation(program, "uYellowColor"), 
                UIColorPalette::PRIMARY_YELLOW[0], 
                UIColorPalette::PRIMARY_YELLOW[1], 
                UIColorPalette::PRIMARY_YELLOW[2], 
                UIColorPalette::PRIMARY_YELLOW[3]);
    glUniform4f(glGetUniformLocation(program, "uOrangeColor"), 
                UIColorPalette::PRIMARY_ORANGE[0], 
                UIColorPalette::PRIMARY_ORANGE[1], 
                UIColorPalette::PRIMARY_ORANGE[2], 
                UIColorPalette::PRIMARY_ORANGE[3]);
    float thickness = m_selected ? 0.6f : 0.5f;
    glUniform1f(glGetUniformLocation(program, "uThickness"), thickness);
    
    // Bind amplitude texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_amplitude_texture);
    glUniform1i(glGetUniformLocation(program, "uAmplitudeTexture"), 0);
    
    // Render full-screen quad (shader will handle clipping to audio region and thickness)
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void TrackRowComponent::set_tape(AudioTape* tape) {
    m_tape = tape;
    m_amplitude_texture_dirty = true;
    // Initialize tracking values
    if (m_tape) {
        m_last_tape_size = m_tape->size();
        m_last_record_position = m_tape->get_current_record_position();
    } else {
        m_last_tape_size = 0;
        m_last_record_position = 0;
    }
    m_update_frame_counter = 0; // Reset counter to allow immediate update
}

void TrackRowComponent::set_timeline_duration(float duration_seconds) {
    m_total_timeline_duration_seconds = duration_seconds;
    m_amplitude_texture_dirty = true;
}

void TrackRowComponent::set_audio_start_offset(float start_offset_seconds) {
    m_audio_start_offset_seconds = start_offset_seconds;
    m_amplitude_texture_dirty = true;
}

void TrackRowComponent::set_selected(bool selected) {
    m_selected = selected;
}

void TrackRowComponent::set_zoom(float zoom) {
    if (zoom > 0.0f) {
        m_zoom = zoom;
    }
}

void TrackRowComponent::set_position(float position_seconds) {
    // Position represents the center of the viewport (position 0 = center at time 0)
    // Clamp to valid range based on current zoom level to keep viewport within bounds
    if (position_seconds >= 0.0f) {
        float visible_duration = MAX_TIMELINE_DURATION_SECONDS / m_zoom;
        float min_center = visible_duration * 0.5f;
        float max_center = MAX_TIMELINE_DURATION_SECONDS - visible_duration * 0.5f;
        
        // Clamp center position to keep viewport within bounds
        // But allow position 0 (which will show from 0 to visible_duration)
        m_position_seconds = std::max(0.0f, std::min(position_seconds, max_center));
    }
}

// ============================================================================
// TrackDisplayComponent Implementation
// ============================================================================

TrackDisplayComponent::TrackDisplayComponent(
    float x, float y, float width, float height,
    PositionMode position_mode,
    size_t num_tracks,
    size_t num_ticks,
    float total_timeline_duration_seconds
) : GraphicsComponent(x, y, width, height, position_mode),
    m_num_tracks(num_tracks),
    m_num_ticks(num_ticks),
    m_total_timeline_duration_seconds(total_timeline_duration_seconds),
    m_measure_component(nullptr),
    m_selected_track_index(-1),
    m_zoom(60.0f),  // Default zoom
    m_position_seconds(0.0f)  // Default position (start at time 0)
{
    // Ensure reasonable defaults
    if (m_num_tracks == 0) m_num_tracks = 6;
    if (m_num_ticks == 0) m_num_ticks = 10;
    // Use max timeline duration of 10 minutes (600 seconds)
    if (m_total_timeline_duration_seconds <= 0.0f) {
        m_total_timeline_duration_seconds = 600.0f;
    }
}

TrackDisplayComponent::~TrackDisplayComponent() {
    // Children are automatically deleted by GraphicsComponent
}

bool TrackDisplayComponent::initialize() {
    // Set custom post-processing fragment shader (shadow vignette on sides)
    const std::string pp_frag = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uTexture;
        void main() {
            vec4 col = texture(uTexture, TexCoord);
            float x = TexCoord.x;
            
            // Left shadow: starts at 0.15 (closer to middle), fades to 0.0 at left edge
            // smoothstep(0.0, 0.25, x) returns 0 at x=0, 1 at x=0.25
            // So 1.0 - smoothstep gives 1 at x=0, 0 at x=0.25
            float left = 1.0 - smoothstep(0.0, 0.25, x);
            
            // Right shadow: starts at 0.85 (closer to middle), fades to 1.0 at right edge
            // smoothstep(0.75, 1.0, x) returns 0 at x=0.75, 1 at x=1.0
            float right = smoothstep(0.75, 1.0, x);
            
            float shadow_intensity = max(left, right);
            
            // Fade to transparent by reducing alpha channel
            // shadow_intensity goes from 0 (center) to 1 (edges)
            // So alpha becomes 0 at the edges (fully transparent)
            float final_alpha = col.a * (1.0 - shadow_intensity);
            
            FragColor = vec4(col.rgb, final_alpha);
        }
    )";
    
    set_post_process_fragment_shader(pp_frag);
    set_post_processing_enabled(true);

    layout_components();
    create_placeholder_data();
    
    // Select the first track by default
    if (m_track_components.size() > 0) {
        select_track(0);
    }
    
    GraphicsComponent::initialize();
    return true;
}

void TrackDisplayComponent::layout_components() {
    // Calculate layout dimensions
    // We'll arrange the measure at the top, then tracks below
    
    float measure_height = m_height * 0.15f; // 15% of height for measure
    float available_height = m_height - measure_height;
    float track_row_height = available_height / static_cast<float>(m_num_tracks);
    float track_width = m_width;
    
    // Get component's top-left corner position
    float comp_x, comp_y;
    get_corner_position(comp_x, comp_y);
    
    // Create measure component at the top with synchronized zoom and position
    // Default BPM is 120 (standard tempo)
    m_measure_component = new TrackMeasureComponent(
        comp_x, comp_y,
        track_width, measure_height,
        PositionMode::TOP_LEFT,
        120.0f,  // Default BPM
        m_zoom,  // Use current zoom value
        m_position_seconds  // Use current position value
    );
    add_child(m_measure_component);
    
    // Create track row components below the measure
    // Use exact calculated positions to ensure all tracks have the same height
    for (size_t i = 0; i < m_num_tracks; ++i) {
        float track_x = comp_x;
        // Calculate exact Y position to ensure consistent spacing
        float track_y = comp_y - measure_height - static_cast<float>(i) * track_row_height;
        
        auto track = new TrackRowComponent(
            track_x, track_y,
            track_width, track_row_height, // All tracks get exactly the same height
            PositionMode::TOP_LEFT,
            nullptr, // No tape initially
            m_total_timeline_duration_seconds,
            m_zoom,  // Use current zoom value
            m_position_seconds  // Use current position value
        );
        m_track_components.push_back(track);
        add_child(track);
    }
}

void TrackDisplayComponent::set_track_tape(size_t track_index, AudioTape* tape) {
    if (track_index < m_track_components.size()) {
        m_track_components[track_index]->set_tape(tape);
    }
}

void TrackDisplayComponent::set_timeline_duration(float duration_seconds) {
    m_total_timeline_duration_seconds = duration_seconds;
    // Update all track components with the new duration
    for (auto* track : m_track_components) {
        track->set_timeline_duration(duration_seconds);
    }
    // Ensure zoom and position remain synchronized after duration change
    synchronize_zoom_and_position();
}

void TrackDisplayComponent::select_track(size_t track_index) {
    // Deselect all tracks
    for (auto* track : m_track_components) {
        track->set_selected(false);
    }
    
    // Select the specified track if valid
    if (track_index < m_track_components.size()) {
        m_track_components[track_index]->set_selected(true);
        m_selected_track_index = static_cast<int>(track_index);
    } else {
        m_selected_track_index = -1;
    }
    // Ensure zoom and position remain synchronized (selection doesn't affect zoom/position, but good practice)
    synchronize_zoom_and_position();
}

void TrackDisplayComponent::synchronize_zoom_and_position() {
    // Synchronize zoom and position across all children
    if (m_measure_component) {
        m_measure_component->set_zoom(m_zoom);
        m_measure_component->set_position(m_position_seconds);
    }
    
    for (auto* track : m_track_components) {
        track->set_zoom(m_zoom);
        track->set_position(m_position_seconds);
    }
}

void TrackDisplayComponent::set_zoom(float zoom) {
    if (zoom > 0.0f) {
        m_zoom = zoom;
        synchronize_zoom_and_position();
    }
}

float TrackDisplayComponent::get_zoom() const {
    return m_zoom;
}

void TrackDisplayComponent::set_position(float position_seconds) {
    // Position represents the center of the viewport (position 0 = center at time 0)
    // Clamp to valid range based on current zoom level to keep viewport within bounds
    if (position_seconds >= 0.0f) {
        float max_duration = TrackRowComponent::MAX_TIMELINE_DURATION_SECONDS;
        float visible_duration = max_duration / m_zoom;
        float max_center = max_duration - visible_duration * 0.5f;
        
        // Clamp center position to keep viewport within bounds
        // Allow position 0 (which will show from 0 to visible_duration, centered at 0)
        m_position_seconds = std::max(0.0f, std::min(position_seconds, max_center));
        synchronize_zoom_and_position();
    }
}

float TrackDisplayComponent::get_position() const {
    return m_position_seconds;
}

void TrackDisplayComponent::create_placeholder_data() {
    // Create placeholder AudioTape objects with different durations
    // to simulate different audio segments on each track
    const unsigned int frames_per_buffer = 512;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    
    m_placeholder_tapes.clear();
    
    // Create different audio segments for each track
    // Track 0: Audio from 0.0 to 2.0 seconds (20% of timeline)
    // Track 1: Audio from 1.0 to 4.0 seconds (30% of timeline, offset)
    // Track 2: Audio from 3.0 to 6.0 seconds (30% of timeline, middle)
    // Track 3: Audio from 5.0 to 8.0 seconds (30% of timeline, later)
    // Track 4: Audio from 7.0 to 9.5 seconds (25% of timeline, near end)
    // Track 5: Audio from 0.5 to 9.0 seconds (85% of timeline, almost full)
    
    struct TrackSegment {
        float start_seconds;
        float duration_seconds;
    };
    
    std::vector<TrackSegment> segments = {
        {0.0f, 2.0f},   // Track 0: 0-2 seconds
        {1.0f, 3.0f},   // Track 1: 1-4 seconds
        {3.0f, 3.0f},   // Track 2: 3-6 seconds
        {5.0f, 3.0f},   // Track 3: 5-8 seconds
        {7.0f, 2.5f},   // Track 4: 7-9.5 seconds
        {0.5f, 8.5f}    // Track 5: 0.5-9 seconds
    };
    
    for (size_t i = 0; i < m_track_components.size() && i < segments.size(); ++i) {
        const auto& seg = segments[i];
        unsigned int samples = static_cast<unsigned int>(seg.duration_seconds * sample_rate);
        
        // Create a tape with the specified size
        auto tape = std::make_shared<AudioTape>(
            frames_per_buffer,
            sample_rate,
            num_channels,
            samples
        );
        
        // Add simulated audio data with varying amplitude
        // Create a waveform that varies in amplitude over time
        std::vector<float> audio_buffer(frames_per_buffer * num_channels);
        unsigned int total_frames = (samples + frames_per_buffer - 1) / frames_per_buffer;
        
        for (unsigned int frame = 0; frame < total_frames; ++frame) {
            unsigned int frame_start_sample = frame * frames_per_buffer;
            unsigned int samples_in_frame = std::min(frames_per_buffer, samples - frame_start_sample);
            
            // Create varying amplitude pattern (sine wave with envelope)
            float t = static_cast<float>(frame_start_sample) / static_cast<float>(samples);
            float envelope = 0.5f + 0.5f * std::sin(t * 3.14159f * 2.0f); // Varying envelope
            float frequency = 440.0f + i * 100.0f; // Different frequency per track
            
            for (unsigned int s = 0; s < samples_in_frame; ++s) {
                float sample_time = static_cast<float>(frame_start_sample + s) / static_cast<float>(sample_rate);
                float wave = std::sin(2.0f * 3.14159f * frequency * sample_time);
                
                // Apply envelope and add some variation
                float amplitude = envelope * (0.3f + 0.7f * std::sin(sample_time * 2.0f + i));
                
                for (unsigned int ch = 0; ch < num_channels; ++ch) {
                    audio_buffer[ch * samples_in_frame + s] = wave * amplitude * 0.5f;
                }
            }
            
            // Record this frame
            tape->record(audio_buffer.data(), static_cast<float>(frame_start_sample) / static_cast<float>(sample_rate));
        }
        
        m_placeholder_tapes.push_back(tape);
        
        // Assign the tape to the track component
        m_track_components[i]->set_tape(tape.get());
        
        // Set the start offset for this track's audio
        m_track_components[i]->set_audio_start_offset(seg.start_seconds);
    }
}

void TrackDisplayComponent::render_content() {
    // This component doesn't render anything itself,
    // it just contains child components that render
    // The base class will handle rendering children
}
