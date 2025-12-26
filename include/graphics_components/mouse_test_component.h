#ifndef MOUSE_TEST_COMPONENT_H
#define MOUSE_TEST_COMPONENT_H

#include <memory>
#include "utilities/shader_program.h"
#include "graphics_core/graphics_component.h"
#include "engine/event_handler.h"
#include "engine/renderable_entity.h"
#include "audio_core/audio_renderer.h"

class MouseTestComponent : public GraphicsComponent {
public:
    MouseTestComponent(PositionMode position_mode, float x, float y, float width, float height);
    MouseTestComponent(float x, float y, float width, float height);
    ~MouseTestComponent() override;

    void render_content() override;

protected:
    bool initialize() override;
    void register_event_handlers(EventHandler* event_handler) override;

private:
    void print_size();
    
    // Convert screen coordinates to normalized device coordinates
    void screen_to_normalized(int screen_x, int screen_y, float& norm_x, float& norm_y);
    
    // State machine for rectangle definition
    enum class State {
        TOP_LEFT,      // State 1: Mouse position sets top-left, no rendering
        BOTTOM_RIGHT,  // State 2: First click set top-left, mouse position sets bottom-right, no rendering
        COMPLETED      // State 3: Second click set bottom-right, shape renders
    };
    
    State m_state = State::TOP_LEFT;
    bool m_size_printed = false;  // Track if size has been printed for current cycle
    
    // Rectangle corners (normalized coordinates)
    // In normalized coordinates: y=1 is top, y=-1 is bottom
    float m_top_left_x = 0.0f;     // Top-left corner X
    float m_top_left_y = 1.0f;     // Top-left corner Y
    float m_bottom_right_x = 0.2f; // Bottom-right corner X (default for initial render)
    float m_bottom_right_y = 0.8f; // Bottom-right corner Y (default for initial render)
    
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_grid_vao = 0;
    GLuint m_grid_vbo = 0;
};

#endif // MOUSE_TEST_COMPONENT_H

