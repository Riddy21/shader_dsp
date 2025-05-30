#include <iostream>
#include <GL/glew.h>
#include "graphics_components/menu_selection_component.h"
#include "engine/event_handler.h"
#include "graphics_core/graphics_display.h"

MenuSelectionComponent::MenuSelectionComponent(
    float x, float y, float width, float height,
    const std::vector<std::string>& items,
    SelectionCallback callback
) : GraphicsComponent(x, y, width, height),
    m_callback(callback)
{
    set_items(items);

    // Create keyboard event handler entries at construction time
    auto key_up_handler = std::make_shared<KeyboardEventHandlerEntry>(
        SDL_KEYDOWN, SDLK_UP,
        [this](const SDL_Event& event) {
            select_previous();
            return true;
        }
    );
    auto key_down_handler = std::make_shared<KeyboardEventHandlerEntry>(
        SDL_KEYDOWN, SDLK_DOWN,
        [this](const SDL_Event& event) {
            select_next();
            return true;
        }
    );
    auto key_return_handler = std::make_shared<KeyboardEventHandlerEntry>(
        SDL_KEYDOWN, SDLK_RETURN,
        [this](const SDL_Event& event) {
            if (m_callback && m_selected_index >= 0) {
                m_callback(m_items[m_selected_index]->get_text());
            }
            return true;
        }
    );
    m_event_handler_entries = {
        key_up_handler,
        key_down_handler,
        key_return_handler
    };
}

MenuSelectionComponent::~MenuSelectionComponent() {
    // Child components are cleaned up by the base class
}

int MenuSelectionComponent::add_item(const std::string& text) {
    int index = static_cast<int>(m_items.size());
    
    // Create a new menu item
    auto* item = new MenuItemComponent(
        m_x,  // Will be updated in update_layout
        m_y,  // Will be updated in update_layout
        m_width, 
        m_item_height,
        text,
        index
    );
    
    // Apply current appearance settings
    item->set_normal_color(m_normal_color[0], m_normal_color[1], m_normal_color[2], m_normal_color[3]);
    item->set_selected_color(m_selected_color[0], m_selected_color[1], m_selected_color[2], m_selected_color[3]);
    item->set_normal_text_color(m_normal_text_color[0], m_normal_text_color[1], m_normal_text_color[2], m_normal_text_color[3]);
    item->set_selected_text_color(m_selected_text_color[0], m_selected_text_color[1], m_selected_text_color[2], m_selected_text_color[3]);
    item->set_font_size(m_font_size);
    if (!m_font_name.empty() && m_font_name != "default") {
        item->set_font(m_font_name);
    }
    
    m_items.push_back(item);
    add_child(item);
    
    // Update the layout to position all items correctly
    update_layout();
    
    // If this is the first item, select it
    if (m_items.size() == 1 && m_selected_index == -1) {
        select_item(0);
    }
    
    return index;
}

void MenuSelectionComponent::set_items(const std::vector<std::string>& items) {
    // Clear existing items
    clear_items();
    
    // Add new items
    for (const auto& text : items) {
        add_item(text);
    }
    
    // Select the first item by default
    if (!items.empty()) {
        select_item(0);
    }
}

void MenuSelectionComponent::clear_items() {
    // Remove all child components (items)
    for (auto* item : m_items) {
        remove_child(item);
    }
    
    m_items.clear();
    m_selected_index = -1;
}

void MenuSelectionComponent::select_item(int index) {
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return;
    }
    
    // Deselect current item
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_items.size())) {
        m_items[m_selected_index]->set_selected(false);
    }
    
    // Select new item
    m_selected_index = index;
    m_items[m_selected_index]->set_selected(true);
    
    // Call the callback if provided
    // TODO: Consider using a signal/slot system instead of direct callback
    if (m_callback) {
        m_callback(m_items[m_selected_index]->get_text());
    }
}

MenuItemComponent* MenuSelectionComponent::get_selected_item() {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_items.size())) {
        return m_items[m_selected_index];
    }
    return nullptr;
}

MenuItemComponent* MenuSelectionComponent::get_item(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        return m_items[index];
    }
    return nullptr;
}

void MenuSelectionComponent::select_next() {
    if (m_items.empty()) return;
    
    int next_index = (m_selected_index + 1) % m_items.size();
    select_item(next_index);
}

void MenuSelectionComponent::select_previous() {
    if (m_items.empty()) return;
    
    int prev_index = (m_selected_index > 0) ? m_selected_index - 1 : m_items.size() - 1;
    select_item(prev_index);
}

void MenuSelectionComponent::update_layout() {
    float total_height = m_item_height * m_items.size() + m_item_padding * (m_items.size() - 1);
    float start_y = m_y + total_height / 2.0f - m_item_height / 2.0f;
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        float item_y = start_y - i * (m_item_height + m_item_padding);
        m_items[i]->set_position(m_x, item_y);
        m_items[i]->set_dimensions(m_width, m_item_height);
    }
}

void MenuSelectionComponent::set_normal_color(float r, float g, float b, float a) {
    m_normal_color[0] = r;
    m_normal_color[1] = g;
    m_normal_color[2] = b;
    m_normal_color[3] = a;
    
    // Apply to all items
    for (auto* item : m_items) {
        item->set_normal_color(r, g, b, a);
    }
}

void MenuSelectionComponent::set_selected_color(float r, float g, float b, float a) {
    m_selected_color[0] = r;
    m_selected_color[1] = g;
    m_selected_color[2] = b;
    m_selected_color[3] = a;
    
    // Apply to all items
    for (auto* item : m_items) {
        item->set_selected_color(r, g, b, a);
    }
}

void MenuSelectionComponent::set_normal_text_color(float r, float g, float b, float a) {
    m_normal_text_color[0] = r;
    m_normal_text_color[1] = g;
    m_normal_text_color[2] = b;
    m_normal_text_color[3] = a;
    
    // Apply to all items
    for (auto* item : m_items) {
        item->set_normal_text_color(r, g, b, a);
    }
}

void MenuSelectionComponent::set_selected_text_color(float r, float g, float b, float a) {
    m_selected_text_color[0] = r;
    m_selected_text_color[1] = g;
    m_selected_text_color[2] = b;
    m_selected_text_color[3] = a;
    
    // Apply to all items
    for (auto* item : m_items) {
        item->set_selected_text_color(r, g, b, a);
    }
}

void MenuSelectionComponent::set_font_size(int size) {
    m_font_size = size;
    
    // Apply to all items
    for (auto* item : m_items) {
        item->set_font_size(size);
    }
}

bool MenuSelectionComponent::set_font(const std::string& font_name) {
    bool success = true;
    m_font_name = font_name;
    
    // Apply to all items
    for (auto* item : m_items) {
        if (!item->set_font(font_name)) {
            success = false;
        }
    }
    
    return success;
}