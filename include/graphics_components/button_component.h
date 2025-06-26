#ifndef BUTTON_COMPONENT_H
#define BUTTON_COMPONENT_H

#include <memory>
#include <string>
#include <functional>
#include <vector>

#include "utilities/shader_program.h"
#include "graphics_core/graphics_component.h"
#include "engine/renderable_item.h"
#include "engine/event_handler.h"
#include "audio_core/audio_renderer.h"

class ButtonComponent : public GraphicsComponent {
public:
    using ButtonCallback = std::function<void()>;

    ButtonComponent(float x, float y, float width, float height, ButtonCallback callback);
    ~ButtonComponent() override;

    void render_content() override;

    void set_callback(ButtonCallback callback);
    void set_colors(float r, float g, float b, float a);
    void set_hover_colors(float r, float g, float b, float a);
    void set_active_colors(float r, float g, float b, float a);
    
    // Check if button is in pressed state (used by the global mouse up handler)
    bool is_pressed() const { return m_is_pressed; }
    
    // Get state for child components to use
    bool is_hovered() const { return m_is_hovered; }
    bool is_active() const { return m_is_pressed; }

    // Apply current button state to child components
    virtual void update_children();

protected:
    ButtonCallback m_callback;
    bool m_is_hovered = false;
    bool m_is_pressed = false;

    float m_color[4] = {0.3f, 0.3f, 0.3f, 1.0f};         // Normal state color
    float m_hover_color[4] = {0.4f, 0.4f, 0.4f, 1.0f};   // Hover state color
    float m_active_color[4] = {0.2f, 0.2f, 0.2f, 1.0f};  // Pressed state color
private:
    void initialize_graphics();

    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
};

#endif // BUTTON_COMPONENT_H