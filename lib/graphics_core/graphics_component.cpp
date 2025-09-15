#include "graphics_core/graphics_component.h"

// Initialize static variables
bool GraphicsComponent::s_global_outline = false;

// Constructor with event handler and render context
GraphicsComponent::GraphicsComponent(
    const float x, 
    const float y, 
    const float width, 
    const float height,
    EventHandler* event_handler,
    const RenderContext& render_context
) : m_x(x), m_y(y), m_width(width), m_height(height), 
    m_event_handler(event_handler), m_render_context(render_context),
    m_initialized(false)
{
    // Register event handlers if the event handler is provided
    if (m_event_handler) {
        register_event_handlers(m_event_handler);
    }
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
    
    // Begin local rendering with viewport and scissor
    begin_local_rendering();
    
    // Render this component's content
    render_content();
    
    // End local rendering
    end_local_rendering();
    
    // Draw debug outline if enabled
    if (m_show_outline || s_global_outline) {
        draw_outline();
    }
    
    // Render all children
    for (auto& child : m_children) {
        child->render();
    }
}

void GraphicsComponent::begin_local_rendering() {
    // Save current viewport and scissor states
    glGetIntegerv(GL_VIEWPORT, m_saved_viewport);
    glGetBooleanv(GL_SCISSOR_TEST, &m_saved_scissor_test);
    if (m_saved_scissor_test) {
        glGetIntegerv(GL_SCISSOR_BOX, m_saved_scissor_box);
    }
    
    // Calculate new viewport based on component position and dimensions
    // Convert from normalized device coordinates to window coordinates
    int window_width = m_saved_viewport[2];
    int window_height = m_saved_viewport[3];
    
    // Calculate component viewport in window coordinates
    int x_pos = static_cast<int>((m_x + 1.0f) * 0.5f * window_width);
    int y_pos = static_cast<int>(((m_y - m_height) + 1.0f) * 0.5f * window_height);
    int comp_width = static_cast<int>(m_width * 0.5f * window_width);
    int comp_height = static_cast<int>(m_height * 0.5f * window_height);

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
    float dx = x - m_x;
    float dy = y - m_y;
    m_x = x;
    m_y = y;
    
    // Update positions of all children
    for (auto& child : m_children) {
        float child_x, child_y;
        child->get_position(child_x, child_y);
        child->set_position(child_x + dx, child_y + dy);
    }
}

void GraphicsComponent::get_position(float& x, float& y) const {
    x = m_x;
    y = m_y;
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