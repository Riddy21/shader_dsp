#include <iostream>
#include <cmath>
#include "graphics_components/mouse_test_component.h"
#include "utilities/shader_program.h"
#include "engine/event_handler.h"

MouseTestComponent::MouseTestComponent(
    PositionMode position_mode,
    float x, 
    float y, 
    float width, 
    float height
) : GraphicsComponent(position_mode, x, y, width, height)
{
    // Start in TOP_LEFT state - shape will render and adjust as mouse moves
}

MouseTestComponent::MouseTestComponent(
    float x, 
    float y, 
    float width, 
    float height
) : MouseTestComponent(PositionMode::CORNER, x, y, width, height)
{
}

MouseTestComponent::~MouseTestComponent() {
    // Clean up OpenGL resources
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_grid_vao != 0) {
        glDeleteVertexArrays(1, &m_grid_vao);
    }
    if (m_grid_vbo != 0) {
        glDeleteBuffers(1, &m_grid_vbo);
    }
}

bool MouseTestComponent::initialize() {
    // Simple vertex and fragment shaders for drawing a rectangle
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
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
        std::cerr << "Failed to initialize shader program for MouseTestComponent" << std::endl;
        return false;
    }

    // Create a rectangle shape (two triangles)
    // Vertices will be updated dynamically based on shape size
    float vertices[] = {
        // Fill (Triangles)
        -1.0f, -1.0f,  // bottom left
        -1.0f,  1.0f,  // top left
         1.0f,  1.0f,  // top right
        
        -1.0f, -1.0f,  // bottom left
         1.0f,  1.0f,  // top right
         1.0f, -1.0f,  // bottom right
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create VAO and VBO for grid lines
    glGenVertexArrays(1, &m_grid_vao);
    glGenBuffers(1, &m_grid_vbo);
    
    glBindVertexArray(m_grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_grid_vbo);
    // Allocate space for grid lines (will be updated dynamically)
    // Grid: 3x3 = 4 vertical lines + 4 horizontal lines = 8 lines = 16 vertices
    glBufferData(GL_ARRAY_BUFFER, 16 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GraphicsComponent::initialize();
    
    // Print initial status
    std::cout << "MouseTestComponent initialized." << std::endl;
    std::cout << "State 1 (TOP_LEFT): Top-left follows mouse, click to cycle." << std::endl;
    std::cout << "State 2 (BOTTOM_RIGHT): Bottom-right follows mouse, click to cycle." << std::endl;
    std::cout << "State 3 (COMPLETED): Both corners fixed, size printed, click to cycle back to State 1." << std::endl;
    
    return true;
}

void MouseTestComponent::screen_to_normalized(int screen_x, int screen_y, float& norm_x, float& norm_y) {
    auto [screen_width, screen_height] = m_render_context.get_size();
    
    // Convert from screen coordinates (0,0 top-left to width,height bottom-right)
    // to normalized device coordinates (-1 to 1, with -1 at top and 1 at bottom)
    norm_x = (screen_x / (float)screen_width) * 2.0f - 1.0f;
    // For Y, screen has 0 at top, but normalized has -1 at top, so we invert
    norm_y = 1.0f - (screen_y / (float)screen_height) * 2.0f;
}

void MouseTestComponent::print_size() {
    float width = m_bottom_right_x - m_top_left_x;
    float height = m_top_left_y - m_bottom_right_y; // Note: top_left_y > bottom_right_y in normalized coords
    std::cout << "Shape Size: (" << width << ", " << height << ")" << std::endl;
}

void MouseTestComponent::register_event_handlers(EventHandler* event_handler) {
    if (!event_handler) return;
    
    // Register mouse motion handler to track position (full screen)
    auto mouse_motion_handler = std::make_shared<MouseMotionEventHandlerEntry>(
        -1.0f, 1.0f, 2.0f, 2.0f,  // Full screen coverage
        [this](const SDL_Event& event) {
            float mouse_x, mouse_y;
            screen_to_normalized(event.motion.x, event.motion.y, mouse_x, mouse_y);
            
            if (m_state == State::TOP_LEFT) {
                // State 1: Top-left corner follows mouse
                m_top_left_x = mouse_x;
                m_top_left_y = mouse_y;
            } else if (m_state == State::BOTTOM_RIGHT) {
                // State 2: Bottom-right corner follows mouse
                m_bottom_right_x = mouse_x;
                m_bottom_right_y = mouse_y;
            }
            // State 3 (COMPLETED): No updates on mouse movement
            
            return true;
        },
        m_render_context
    );

    // Register mouse button down handler (full screen)
    auto mouse_down_handler = std::make_shared<MouseClickEventHandlerEntry>(
        SDL_MOUSEBUTTONDOWN,
        -1.0f, 1.0f, 2.0f, 2.0f,  // Full screen coverage
        [this](const SDL_Event& event) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                float click_x, click_y;
                screen_to_normalized(event.button.x, event.button.y, click_x, click_y);
                
                if (m_state == State::TOP_LEFT) {
                    // Click in TOP_LEFT: Set top-left corner and cycle to BOTTOM_RIGHT state
                    m_top_left_x = click_x;
                    m_top_left_y = click_y;
                    m_state = State::BOTTOM_RIGHT;
                    m_size_printed = false; // Reset print flag for new cycle
                    std::cout << "Top-left corner set to (" << click_x << ", " << click_y << ")" << std::endl;
                } else if (m_state == State::BOTTOM_RIGHT) {
                    // Click in BOTTOM_RIGHT: Set bottom-right corner and cycle to COMPLETED state
                    m_bottom_right_x = click_x;
                    m_bottom_right_y = click_y;
                    m_state = State::COMPLETED;
                    std::cout << "Bottom-right corner set to (" << click_x << ", " << click_y << ")" << std::endl;
                    if (!m_size_printed) {
                        print_size(); // Print size once when entering COMPLETED
                        m_size_printed = true;
                    }
                } else if (m_state == State::COMPLETED) {
                    // Click in COMPLETED: Cycle back to TOP_LEFT state
                    m_state = State::TOP_LEFT;
                    m_size_printed = false; // Reset print flag for new cycle
                    std::cout << "Cycling back to TOP_LEFT state." << std::endl;
                }
            }
            return true;
        },
        m_render_context
    );

    // Add handlers to the component's event handler entries BEFORE calling parent
    m_event_handler_entries.push_back(mouse_motion_handler);
    m_event_handler_entries.push_back(mouse_down_handler);
    
    // Now call parent to register all handlers
    GraphicsComponent::register_event_handlers(event_handler);
}

void MouseTestComponent::render_content() {
    if (!m_shader_program || m_vao == 0) return;
    
    // Render in all states - shape adjusts based on current state
    glUseProgram(m_shader_program->get_program());
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set color (cyan with some transparency)
    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                0.0f, 0.8f, 1.0f, 0.7f);
    
    // Update vertices based on top-left and bottom-right corners
    // In normalized coordinates: y=1 is top, y=-1 is bottom
    // So top_left_y > bottom_right_y
    float vertices[] = {
        // Fill (Triangles) - rectangle from top-left to bottom-right
        m_top_left_x, m_bottom_right_y,      // bottom left
        m_top_left_x, m_top_left_y,        // top left
        m_bottom_right_x, m_top_left_y,    // top right
        
        m_top_left_x, m_bottom_right_y,     // bottom left
        m_bottom_right_x, m_top_left_y,    // top right
        m_bottom_right_x, m_bottom_right_y // bottom right
    };
    
    // Update the VBO with new vertices
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    
    // Draw the shape
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Draw grid lines
    // Calculate grid positions (3x3 grid = 4 lines in each direction)
    float width = m_bottom_right_x - m_top_left_x;
    float height = m_top_left_y - m_bottom_right_y;
    
    // Grid line vertices (3x3 grid = 2 vertical + 2 horizontal center lines)
    // Vertical lines (2 lines dividing into 3 columns)
    // Horizontal lines (2 lines dividing into 3 rows)
    float grid_vertices[16 * 2]; // 8 lines * 2 vertices per line * 2 floats per vertex
    
    int idx = 0;
    
    // Vertical lines (2 lines)
    for (int i = 1; i < 3; i++) {
        float x = m_top_left_x + (width / 3.0f) * i;
        // Top to bottom
        grid_vertices[idx++] = x;
        grid_vertices[idx++] = m_top_left_y;
        grid_vertices[idx++] = x;
        grid_vertices[idx++] = m_bottom_right_y;
    }
    
    // Horizontal lines (2 lines)
    for (int i = 1; i < 3; i++) {
        float y = m_top_left_y - (height / 3.0f) * i;
        // Left to right
        grid_vertices[idx++] = m_top_left_x;
        grid_vertices[idx++] = y;
        grid_vertices[idx++] = m_bottom_right_x;
        grid_vertices[idx++] = y;
    }
    
    // Update grid VBO
    glBindVertexArray(m_grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_grid_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(grid_vertices), grid_vertices);
    
    // Set grid color (darker/white lines)
    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                1.0f, 1.0f, 1.0f, 0.9f); // White with high opacity
    
    // Draw grid lines
    glLineWidth(1.5f);
    glDrawArrays(GL_LINES, 0, 8); // 8 lines = 16 vertices
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    
    glUseProgram(0);
}

