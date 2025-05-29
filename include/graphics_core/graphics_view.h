#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

#include <graphics_core/graphics_component.h>
#include <engine/event_handler.h>
#include <engine/renderable_item.h>

class GraphicsView {
public:
    // Constructor that sets up the view with event handler and render context
    GraphicsView(EventHandler& event_handler, const RenderContext& render_context);
    
    virtual ~GraphicsView();
    
    virtual void render();
    
    // View lifecycle hooks
    virtual void on_enter();
    virtual void on_exit();

    // Component management
    void add_component(GraphicsComponent* component);
    void remove_component(GraphicsComponent* component);
    
    // Get the render context
    const RenderContext& get_render_context() const { return m_render_context; }
    
protected:
    void register_event_handler(EventHandler& event_handler);
    void unregister_event_handler(EventHandler& event_handler);

    EventHandler* m_event_handler = nullptr; // Reference to the event handler
    RenderContext m_render_context; // Render context for this view
    std::vector<std::unique_ptr<GraphicsComponent>> m_components;

    bool m_event_handlers_registered = false; // Flag to prevent double registration
};
