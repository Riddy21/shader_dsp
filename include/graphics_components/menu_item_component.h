#ifndef MENU_ITEM_COMPONENT_H
#define MENU_ITEM_COMPONENT_H

#include <memory>
#include <string>
#include <functional>
#include "graphics_components/text_component.h"
#include "graphics_core/graphics_component.h"

// Forward declaration
class MenuSelectionComponent;

class MenuItemComponent : public GraphicsComponent {
public:
    using SelectionCallback = std::function<void(std::string)>;
    
    MenuItemComponent(
        PositionMode position_mode, float x, float y, float width, float height,
        const std::string& text, int item_index
    );
    MenuItemComponent(
        float x, float y, float width, float height,
        const std::string& text, int item_index
    );
    ~MenuItemComponent() override;

    void render_content() override;

    // Item state management
    void set_selected(bool selected);
    bool is_selected() const { return m_is_selected; }
    
    // Text handling
    void set_text(const std::string& text);
    const std::string& get_text() const;
    
    // Appearance customization
    void set_normal_color(float r, float g, float b, float a);
    void set_selected_color(float r, float g, float b, float a);
    void set_normal_text_color(float r, float g, float b, float a);
    void set_selected_text_color(float r, float g, float b, float a);
    
    void set_font_size(int size);
    bool set_font(const std::string& font_name);

    // Index management
    int get_index() const { return m_index; }
    void set_index(int index) { m_index = index; }

protected:
    // Override the base class initialization
    bool initialize() override;
    
private:
    void update_colors();

    TextComponent* m_text_component;
    int m_index;
    bool m_is_selected = false;
    bool m_colors_dirty = true; // <-- Add this flag
    
    // Background colors
    float m_normal_color[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    float m_selected_color[4] = {0.4f, 0.4f, 0.6f, 1.0f};
    
    // Text colors
    float m_normal_text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_selected_text_color[4] = {1.0f, 1.0f, 0.8f, 1.0f};
    
    // Static shader program shared by all menu items
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    
    // Instance-specific OpenGL resources
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
};

#endif // MENU_ITEM_COMPONENT_H