#ifndef MENU_SELECTION_COMPONENT_H
#define MENU_SELECTION_COMPONENT_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "graphics_core/graphics_component.h"
#include "graphics_components/menu_item_component.h"

class MenuSelectionComponent : public GraphicsComponent {
public:
    using SelectionCallback = std::function<void(int)>;
    
    MenuSelectionComponent(
        float x, float y, float width, float height,
        const std::vector<std::string>& items,
        SelectionCallback callback = nullptr
    );
    ~MenuSelectionComponent() override;

    // Item management
    int add_item(const std::string& text);
    void set_items(const std::vector<std::string>& items);
    void clear_items();
    size_t get_item_count() const { return m_items.size(); }
    
    // Selection management
    void select_item(int index);
    int get_selected_index() const { return m_selected_index; }
    MenuItemComponent* get_selected_item();
    MenuItemComponent* get_item(int index);
    
    void set_callback(SelectionCallback callback) { m_callback = callback; }
    
    // Navigation
    void select_next();
    void select_previous();
    
    // Appearance customization
    void set_item_padding(float padding) { m_item_padding = padding; update_layout(); }
    void set_item_height(float height) { m_item_height = height; update_layout(); }
    
    void set_normal_color(float r, float g, float b, float a);
    void set_selected_color(float r, float g, float b, float a);
    void set_normal_text_color(float r, float g, float b, float a);
    void set_selected_text_color(float r, float g, float b, float a);
    
    void set_font_size(int size);
    bool set_font(const std::string& font_name);

private:
    void update_layout();
    
    std::vector<MenuItemComponent*> m_items;
    int m_selected_index = -1;
    SelectionCallback m_callback;
    
    float m_item_height = .50f;
    float m_item_padding = 0.00f;
    
    // Appearance settings (passed to items)
    float m_normal_color[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    float m_selected_color[4] = {0.4f, 0.4f, 0.6f, 1.0f};
    float m_normal_text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_selected_text_color[4] = {1.0f, 1.0f, 0.8f, 1.0f};
    int m_font_size = 16;
    std::string m_font_name = "default";
};

#endif // MENU_SELECTION_COMPONENT_H