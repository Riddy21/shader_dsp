#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

#include "engine/event_handler.h"
#include "engine/renderable_item.h"

class GraphicsComponent {
public:
    GraphicsComponent(
        const float x = 0.0f, 
        const float y = 0.0f, 
        const float width = 0.0f, 
        const float height = 0.0f
    );
    virtual ~GraphicsComponent() = default;
    
    virtual void render();

    virtual void register_event_handlers(EventHandler* event_handler);
    virtual void unregister_event_handlers();

    void set_position(const float x, const float y);
    void get_position(float& x, float& y) const;
    void set_dimensions(const float width, const float height);
    void get_dimensions(float& width, float& height) const;
    void set_display_id(unsigned int id);

    // Adding a child component (takes ownership)
    void add_child(GraphicsComponent* child);
    
    // Removing a child component (releases ownership)
    void remove_child(GraphicsComponent* child);
    
    // Gets a pointer to a child (does not transfer ownership)
    GraphicsComponent* get_child(size_t index) const;
    
    // Gets the number of children
    size_t get_child_count() const;
    
protected:
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_width = 0.0f;
    float m_height = 0.0f;
    unsigned int m_window_id = 0; // Window ID for this component

    // Event handler entries
    EventHandler* m_event_handler = nullptr;
    std::vector<EventHandlerEntry*> m_event_handler_entries;
    bool m_event_handlers_registered = false; // Flag to prevent double registration
    
    // Child components (owned by this component)
    std::vector<std::unique_ptr<GraphicsComponent>> m_children;
};