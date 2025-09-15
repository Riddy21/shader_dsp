#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

#include <graphics_core/graphics_component.h>
#include <engine/event_handler.h>
#include <engine/renderable_entity.h>

class GraphicsView {
public:
    // Constructor that sets up the view
    GraphicsView();
    virtual ~GraphicsView();

    // Initialize the view with event handler and render context
    virtual void initialize(EventHandler& event_handler, const RenderContext& render_context);
    
    // Legacy initialization for backward compatibility
    virtual void initialize(EventHandler& event_handler, unsigned int display_id);
    
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
    void set_event_handler(EventHandler& event_handler);
    void set_render_context(const RenderContext& render_context);
    
    // Legacy method - use set_render_context instead
    void set_display_id(unsigned int id);

    void register_event_handler(EventHandler& event_handler);
    void unregister_event_handler(EventHandler& event_handler);

    EventHandler* m_event_handler = nullptr; // Reference to the event handler
    RenderContext m_render_context; // Render context for this view
    std::vector<std::unique_ptr<GraphicsComponent>> m_components;
    bool m_components_initialized = false; // Add this flag

    bool m_event_handlers_registered = false; // Flag to prevent double registration
};
