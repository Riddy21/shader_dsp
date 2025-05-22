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
    
    virtual void render();
    
    // View lifecycle hooks
    virtual void on_enter();
    virtual void on_exit();

    // Component management
    void add_component(GraphicsComponent* component);
    void remove_component(GraphicsComponent* component);

    // Set the event handler to use for this view
    void set_event_handler(EventHandler* event_handler);
    EventHandler* get_event_handler() const { return m_event_handler; }
    
    // Set the parent display (which is the render context for event handlers)
    void set_parent_display(IRenderableEntity* parent) { m_parent_display = parent; }
    IRenderableEntity* get_parent_display() const { return m_parent_display; }

protected:
    // Event handler management
    virtual void register_event_handler(EventHandler* event_handler);
    virtual void unregister_event_handler(EventHandler* event_handler);

    EventHandler* m_event_handler;
    std::vector<EventHandlerEntry*> m_registered_entries;
    std::vector<std::unique_ptr<GraphicsComponent>> m_components;

    IRenderableEntity* m_parent_display = nullptr;
    
};
