#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

#include <graphics_core/graphics_component.h>
#include <engine/event_handler.h>

class GraphicsView {
public:
    GraphicsView();
    virtual ~GraphicsView();

    virtual void initialize(EventHandler& event_handler, unsigned int display_id);
    
    virtual void render();
    
    // View lifecycle hooks
    virtual void on_enter();
    virtual void on_exit();

    // Component management
    void add_component(GraphicsComponent* component);
    void remove_component(GraphicsComponent* component);
protected:
    void set_event_handler(EventHandler& event_handler);
    void set_display_id(unsigned int id);

    void register_event_handler(EventHandler& event_handler);
    void unregister_event_handler(EventHandler& event_handler);

    EventHandler * m_event_handler = nullptr; // Reference to the event handler, cannot be re-assigned
    unsigned int m_display_id = 0; // Unique ID for the display this view is associated with
    std::vector<std::unique_ptr<GraphicsComponent>> m_components;

    bool m_event_handlers_registered = false; // Flag to prevent double registration
};
