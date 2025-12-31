#include "graphics_core/graphics_component.h"
#include <iostream>

// Initialize static variables
bool GraphicsComponent::s_global_outline = false;
int GraphicsComponent::s_viewport_offset_x = 0;
int GraphicsComponent::s_viewport_offset_y = 0;

// Track the global window size for correct FBO mapping
static int s_global_window_width = 0;
static int s_global_window_height = 0;

// Constructor with event handler and render context
GraphicsComponent::GraphicsComponent(
    const float x, 
    const float y, 
    const float width, 
    const float height,
    PositionMode position_mode,
    EventHandler* event_handler,
    const RenderContext& render_context
) : m_width(width), m_height(height), m_position_mode(position_mode),
    m_event_handler(event_handler), m_render_context(render_context),
    m_initialized(false)
{
    // Convert position based on mode - always store as corner coordinates
    switch (position_mode) {
        case PositionMode::CENTER:
            m_x = x - width * 0.5f;
            m_y = y + height * 0.5f;
            break;
        case PositionMode::CENTER_BOTTOM:
            m_x = x - width * 0.5f;
            m_y = y + height;
            break;
        case PositionMode::CENTER_TOP:
            m_x = x - width * 0.5f;
            m_y = y;
            break;
        case PositionMode::TOP_RIGHT:
            m_x = x - width;
            m_y = y;
            break;
        case PositionMode::BOTTOM_LEFT:
            m_x = x;
            m_y = y + height;
            break;
        case PositionMode::BOTTOM_RIGHT:
            m_x = x - width;
            m_y = y + height;
            break;
        case PositionMode::TOP_LEFT:
        default:
            m_x = x;
            m_y = y;
            break;
    }
    
    // Register event handlers if the event handler is provided
    if (m_event_handler) {
        register_event_handlers(m_event_handler);
    }
}

GraphicsComponent::~GraphicsComponent() {
    cleanup_fbo();
}

bool GraphicsComponent::initialize() {
    for (auto& child : m_children) {
        child->initialize();
    }

    m_initialized = true;

    return true;
}


void GraphicsComponent::render() {
    // Skip rendering if dimensions are zero
    if (m_width <= 0 || m_height <= 0) return;
    
    if (m_post_processing_enabled) {
        prepare_fbo();
        
        // Save previous FBO and viewport
        GLint prev_fbo;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        GLint prev_viewport[4];
        glGetIntegerv(GL_VIEWPORT, prev_viewport);
        
        // Calculate component position in window pixels for global offset
        int win_w = prev_viewport[2]; // Assuming current viewport covers window or is parent's viewport
        int win_h = prev_viewport[3];
        
        // Note: begin_local_rendering uses m_saved_viewport to calculate pixels.
        // We can reuse the logic but we need the result here.
        // Actually, we can just let begin_local_rendering calculate it if we didn't offset yet.
        // But we need to offset it.
        
        // Calculate component viewport in window coordinates (absolute)
        int x_pos = static_cast<int>((m_x + 1.0f) * 0.5f * win_w);
        int y_pos = static_cast<int>(((m_y - m_height) + 1.0f) * 0.5f * win_h);
        
        // Update global offset
        int old_offset_x = s_viewport_offset_x;
        int old_offset_y = s_viewport_offset_y;
        
        s_viewport_offset_x += x_pos;
        s_viewport_offset_y += y_pos;
        
        // Bind FBO
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_fbo_width, m_fbo_height);
        
        // Clear FBO
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent
        glClear(GL_COLOR_BUFFER_BIT); // No depth buffer by default unless added
        
        // Render content to FBO
        // Note: We need to use begin_local_rendering here to set up scissor/viewport inside FBO
        begin_local_rendering();
        render_content();
        end_local_rendering();
        
        // Render children to FBO
        for (auto& child : m_children) {
            child->render();
        }
        
        // Restore state
        s_viewport_offset_x = old_offset_x;
        s_viewport_offset_y = old_offset_y;
        
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
        
        // Now render the result to screen
        // We use begin_local_rendering to target the component's screen area
        begin_local_rendering();
        render_post_process();
        end_local_rendering();
        
        // Debug outline on top of post-process result
        if (m_show_outline || s_global_outline) {
            draw_outline();
        }
        
    } else {
        // Standard rendering path
        begin_local_rendering();
        
        render_content();
        
        end_local_rendering();
        
        if (m_show_outline || s_global_outline) {
            draw_outline();
        }
        
        for (auto& child : m_children) {
            child->render();
        }
    }
}

void GraphicsComponent::begin_local_rendering() {
    // Save current viewport and scissor states
    glGetIntegerv(GL_VIEWPORT, m_saved_viewport);
    glGetBooleanv(GL_SCISSOR_TEST, &m_saved_scissor_test);
    if (m_saved_scissor_test) {
        glGetIntegerv(GL_SCISSOR_BOX, m_saved_scissor_box);
    }
    
    // Determine the reference width/height for NDC conversion.
    // If we are at the top level (no offset), we capture the global window size.
    // If we are rendering into an FBO (offset > 0), we must use the global window size
    // to correctly map the component's global NDC coordinates to local pixels.
    
    int ref_width = m_saved_viewport[2];
    int ref_height = m_saved_viewport[3];
    
    if (s_viewport_offset_x == 0 && s_viewport_offset_y == 0) {
        // Top-level rendering - store the global window size
        s_global_window_width = ref_width;
        s_global_window_height = ref_height;
    } else {
        // Nested rendering (likely inside FBO) - use the stored global size
        // If s_global_window_width is 0 for some reason (shouldn't happen if top-level called render), fallback
        if (s_global_window_width > 0) {
            ref_width = s_global_window_width;
            ref_height = s_global_window_height;
        }
    }
    
    // Calculate new viewport based on component position and dimensions
    // Convert from normalized device coordinates to window coordinates
    // Using ref_width/height ensures consistency between global and local rendering
    int x_pos = static_cast<int>((m_x + 1.0f) * 0.5f * ref_width);
    int y_pos = static_cast<int>(((m_y - m_height) + 1.0f) * 0.5f * ref_height);
    int comp_width = static_cast<int>(m_width * 0.5f * ref_width);
    int comp_height = static_cast<int>(m_height * 0.5f * ref_height);

    // Apply global offset (used when rendering to FBO)
    // This transforms global window coordinates to local FBO coordinates
    x_pos -= s_viewport_offset_x;
    y_pos -= s_viewport_offset_y;

    // Set viewport to only render within the component's area
    glViewport(x_pos, y_pos, comp_width, comp_height);
    
    // Use scissors to restrict drawing to component area
    glEnable(GL_SCISSOR_TEST);
    glScissor(x_pos, y_pos, comp_width, comp_height);
}

void GraphicsComponent::end_local_rendering() {
    // Restore original viewport
    glViewport(m_saved_viewport[0], m_saved_viewport[1], 
               m_saved_viewport[2], m_saved_viewport[3]);
    
    // Restore original scissor state
    if (m_saved_scissor_test) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(m_saved_scissor_box[0], m_saved_scissor_box[1],
                  m_saved_scissor_box[2], m_saved_scissor_box[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GraphicsComponent::render_content() {
    // Base implementation does nothing - derived classes should override this
}

void GraphicsComponent::register_event_handlers(EventHandler* event_handler) {
    if (m_event_handlers_registered) return;
    m_event_handler = event_handler;
    for (auto& entry : m_event_handler_entries) {
        m_event_handler->register_entry(entry);
    }
    for (auto& child : m_children) {
        child->register_event_handlers(event_handler);
    }
    m_event_handlers_registered = true;
}

void GraphicsComponent::unregister_event_handlers() {
    if (!m_event_handlers_registered) return;
    if (m_event_handler) {
        for (auto& entry : m_event_handler_entries) {
            m_event_handler->unregister_entry(entry);
        }
    }
    for (auto& child : m_children) {
        child->unregister_event_handlers();
    }
    m_event_handlers_registered = false;
}

void GraphicsComponent::set_position(const float x, const float y) {
    float old_corner_x = m_x;
    float old_corner_y = m_y;
    
    // Convert input position to corner coordinates based on mode
    switch (m_position_mode) {
        case PositionMode::CENTER:
            m_x = x - m_width * 0.5f;
            m_y = y + m_height * 0.5f;
            break;
        case PositionMode::CENTER_BOTTOM:
            m_x = x - m_width * 0.5f;
            m_y = y + m_height;
            break;
        case PositionMode::CENTER_TOP:
            m_x = x - m_width * 0.5f;
            m_y = y;
            break;
        case PositionMode::TOP_RIGHT:
            m_x = x - m_width;
            m_y = y;
            break;
        case PositionMode::BOTTOM_LEFT:
            m_x = x;
            m_y = y + m_height;
            break;
        case PositionMode::BOTTOM_RIGHT:
            m_x = x - m_width;
            m_y = y + m_height;
            break;
        case PositionMode::TOP_LEFT:
        default:
            m_x = x;
            m_y = y;
            break;
    }
    
    // Calculate delta for children
    float dx = m_x - old_corner_x;
    float dy = m_y - old_corner_y;
    
    // Update positions of all children
    for (auto& child : m_children) {
        float child_x, child_y;
        child->get_corner_position(child_x, child_y);
        child->set_position(child_x + dx, child_y + dy);
    }
}

void GraphicsComponent::get_position(float& x, float& y) const {
    // Return position based on current mode
    switch (m_position_mode) {
        case PositionMode::CENTER:
            x = m_x + m_width * 0.5f;
            y = m_y - m_height * 0.5f;
            break;
        case PositionMode::CENTER_BOTTOM:
            x = m_x + m_width * 0.5f;
            y = m_y - m_height;
            break;
        case PositionMode::CENTER_TOP:
            x = m_x + m_width * 0.5f;
            y = m_y;
            break;
        case PositionMode::TOP_RIGHT:
            x = m_x + m_width;
            y = m_y;
            break;
        case PositionMode::BOTTOM_LEFT:
            x = m_x;
            y = m_y - m_height;
            break;
        case PositionMode::BOTTOM_RIGHT:
            x = m_x + m_width;
            y = m_y - m_height;
            break;
        case PositionMode::TOP_LEFT:
        default:
            x = m_x;
            y = m_y;
            break;
    }
}

void GraphicsComponent::get_corner_position(float& x, float& y) const {
    x = m_x;
    y = m_y;
}

void GraphicsComponent::get_center_position(float& x, float& y) const {
    x = m_x + m_width * 0.5f;
    y = m_y - m_height * 0.5f;  // In normalized coords, subtract half height
}

void GraphicsComponent::set_position_mode(PositionMode mode) {
    if (mode == m_position_mode) return;
    
    // Convert current position to the new mode
    float current_x, current_y;
    get_position(current_x, current_y);
    
    m_position_mode = mode;
    
    // Set position again with new mode (will convert correctly)
    set_position(current_x, current_y);
}

void GraphicsComponent::set_dimensions(const float width, const float height) {
    float width_ratio = width / m_width;
    float height_ratio = height / m_height;
    m_width = width;
    m_height = height;

    // Update positions and dimensions of all children to fit the new ratio
    for (auto& child : m_children) {
        float child_x, child_y, child_width, child_height;
        child->get_position(child_x, child_y);
        child->get_dimensions(child_width, child_height);

        float new_child_x = m_x + (child_x - m_x) * width_ratio;
        float new_child_y = m_y + (child_y - m_y) * height_ratio;
        float new_child_width = child_width * width_ratio;
        float new_child_height = child_height * height_ratio;

        child->set_position(new_child_x, new_child_y);
        child->set_dimensions(new_child_width, new_child_height);
    }
}

void GraphicsComponent::get_dimensions(float& width, float& height) const {
    width = m_width;
    height = m_height;
}

void GraphicsComponent::set_render_context(const RenderContext& render_context) {
    m_render_context = render_context;
    
    // Update window ID in event handler entries
    for (auto& entry : m_event_handler_entries) {
        entry->set_render_context(render_context);
    }
    
    // Update render context for all children
    for (auto& child : m_children) {
        child->set_render_context(render_context);
    }
}

// Legacy method for backward compatibility
void GraphicsComponent::set_display_id(unsigned int id) {
    // Create a temporary RenderContext with just the window_id set
    RenderContext ctx;
    ctx.window_id = id;
    
    // Update the context for this component and all children
    set_render_context(ctx);
}

void GraphicsComponent::add_child(GraphicsComponent* child) {
    // Take ownership of the child component
    m_children.push_back(std::unique_ptr<GraphicsComponent>(child));
    
    // Register event handlers for the child if we're already registered
    if (m_event_handlers_registered && m_event_handler) {
        child->register_event_handlers(m_event_handler);
    }
}

void GraphicsComponent::remove_child(GraphicsComponent* child) {
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (it->get() == child) {
            // Unregister event handlers for the child if we're registered
            if (m_event_handlers_registered) {
                (*it)->unregister_event_handlers();
            }
            
            // Release ownership and return the raw pointer
            it->release();
            m_children.erase(it);
            break;
        }
    }
}

GraphicsComponent* GraphicsComponent::get_child(size_t index) const {
    if (index < m_children.size()) {
        return m_children[index].get();
    }
    return nullptr;
}

size_t GraphicsComponent::get_child_count() const {
    return m_children.size();
}

void GraphicsComponent::set_outline_color(float r, float g, float b, float a) {
    m_outline_color[0] = r;
    m_outline_color[1] = g;
    m_outline_color[2] = b;
    m_outline_color[3] = a;
}

void GraphicsComponent::draw_outline() {
    // Calculate the corners of the rectangle in our coordinate system
    // where (-1,1) is top-left and (1,-1) is bottom-right
    float x1 = m_x;
    float y1 = m_y;
    float x2 = m_x + m_width;
    float y2 = m_y - m_height;
    
    // 4 lines, 2 points per line, 2 floats per point
    const float line_vertices[] = {
        // Bottom line: bottom-left to bottom-right
        x1, y1, x2, y1,
        // Right line: bottom-right to top-right
        x2, y1, x2, y2,
        // Top line: top-right to top-left
        x2, y2, x1, y2,
        // Left line: top-left to bottom-left
        x1, y2, x1, y1
    };
    
    // Save GL state
    GLint previous_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    GLboolean blend_enabled;
    glGetBooleanv(GL_BLEND, &blend_enabled);
    GLint blend_src, blend_dst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);
    
    // Create a simple line VAO and VBO on the fly
    GLuint line_vao, line_vbo;
    glGenVertexArrays(1, &line_vao);
    glGenBuffers(1, &line_vbo);
    
    glBindVertexArray(line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // We'll use a simple pass-through shader
    const char* vert_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";
    
    const char* frag_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";
    
    // Compile shaders
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_src, NULL);
    glCompileShader(vert_shader);
    
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_src, NULL);
    glCompileShader(frag_shader);
    
    // Create program
    GLuint outline_program = glCreateProgram();
    glAttachShader(outline_program, vert_shader);
    glAttachShader(outline_program, frag_shader);
    glLinkProgram(outline_program);
    
    // Use program and set color
    glUseProgram(outline_program);
    glUniform4f(glGetUniformLocation(outline_program, "uColor"), 
                m_outline_color[0], m_outline_color[1], m_outline_color[2], m_outline_color[3]);
    
    // Setup for line drawing
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw the outline
    glBindVertexArray(line_vao);
    glDrawArrays(GL_LINES, 0, 8); // 4 lines, 2 points each
    
    // Cleanup
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteVertexArrays(1, &line_vao);
    glDeleteBuffers(1, &line_vbo);
    glDeleteProgram(outline_program);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    
    // Restore GL state
    glUseProgram(previous_program);
    if (!blend_enabled) {
        glDisable(GL_BLEND);
    } else {
        glBlendFunc(blend_src, blend_dst);
    }
}

void GraphicsComponent::set_post_processing_enabled(bool enabled) {
    m_post_processing_enabled = enabled;
}

void GraphicsComponent::set_post_process_fragment_shader(const std::string& fragment_shader_src) {
    m_custom_post_frag_shader = fragment_shader_src;
    
    // If post-processing resources are already initialized, recompile the shader
    if (m_post_program != 0) {
        // Delete old program
        glDeleteProgram(m_post_program);
        m_post_program = 0;
        
        // Recompile with new shader
        compile_post_process_shader();
    }
}

void GraphicsComponent::prepare_fbo() {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int window_width = viewport[2];
    int window_height = viewport[3];

    // Calculate component pixel size
    int pixel_width = static_cast<int>(m_width * 0.5f * window_width);
    int pixel_height = static_cast<int>(m_height * 0.5f * window_height);
    
    // Ensure minimum size
    if (pixel_width <= 0) pixel_width = 1;
    if (pixel_height <= 0) pixel_height = 1;

    // Check if resize needed
    if (m_fbo == 0 || pixel_width != m_fbo_width || pixel_height != m_fbo_height) {
        m_fbo_width = pixel_width;
        m_fbo_height = pixel_height;
        
        if (m_fbo == 0) {
            glGenFramebuffers(1, &m_fbo);
            glGenTextures(1, &m_texture);
            initialize_post_process_resources();
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fbo_width, m_fbo_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer is not complete!" << std::endl;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void GraphicsComponent::cleanup_fbo() {
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (m_post_vao != 0) {
        glDeleteVertexArrays(1, &m_post_vao);
        m_post_vao = 0;
    }
    if (m_post_vbo != 0) {
        glDeleteBuffers(1, &m_post_vbo);
        m_post_vbo = 0;
    }
    if (m_post_program != 0) {
        glDeleteProgram(m_post_program);
        m_post_program = 0;
    }
}

void GraphicsComponent::initialize_post_process_resources() {
    // Create quad for post-process rendering
    float vertices[] = {
        // x, y, u, v
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_post_vao);
    glGenBuffers(1, &m_post_vbo);
    
    glBindVertexArray(m_post_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_post_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Compile the post-process shader
    compile_post_process_shader();
}

void GraphicsComponent::compile_post_process_shader() {
    // Standard vertex shader for post-processing
    const char* vert_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    
    // Use custom fragment shader if provided, otherwise use default pass-through
    const char* frag_src;
    std::string default_frag = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uTexture;
        void main() {
            FragColor = texture(uTexture, TexCoord);
        }
    )";
    
    if (!m_custom_post_frag_shader.empty()) {
        frag_src = m_custom_post_frag_shader.c_str();
    } else {
        frag_src = default_frag.c_str();
    }
    
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vert_src, NULL);
    glCompileShader(vert);
    
    // Check vertex shader compilation
    GLint success;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vert, 512, NULL, info_log);
        std::cerr << "Post-process vertex shader compilation failed: " << info_log << std::endl;
        glDeleteShader(vert);
        return;
    }
    
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &frag_src, NULL);
    glCompileShader(frag);
    
    // Check fragment shader compilation
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(frag, 512, NULL, info_log);
        std::cerr << "Post-process fragment shader compilation failed: " << info_log << std::endl;
        glDeleteShader(vert);
        glDeleteShader(frag);
        return;
    }
    
    m_post_program = glCreateProgram();
    glAttachShader(m_post_program, vert);
    glAttachShader(m_post_program, frag);
    glLinkProgram(m_post_program);
    
    // Check program linking
    glGetProgramiv(m_post_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(m_post_program, 512, NULL, info_log);
        std::cerr << "Post-process shader program linking failed: " << info_log << std::endl;
        glDeleteProgram(m_post_program);
        m_post_program = 0;
    }
    
    glDeleteShader(vert);
    glDeleteShader(frag);
}

void GraphicsComponent::render_post_process() {
    if (m_post_program == 0 || m_post_vao == 0) return;
    
    glUseProgram(m_post_program);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(glGetUniformLocation(m_post_program, "uTexture"), 0);
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(m_post_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glDisable(GL_BLEND);
    glUseProgram(0);
}
