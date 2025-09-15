#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

#include "engine/event_handler.h"
#include "engine/renderable_entity.h"

// FIXME: Think about whether an event handler and render context should be passed for every component
class GraphicsComponent {
public:
    // Constructor with position, dimensions, event handler, and render context
    GraphicsComponent(
        const float x = 0.0f, 
        const float y = 0.0f, 
        const float width = 0.0f, 
        const float height = 0.0f,
        EventHandler* event_handler = nullptr,
        const RenderContext& render_context = RenderContext()
    );
    
    virtual ~GraphicsComponent() = default;

    // Event handler registration methods
    virtual void register_event_handlers(EventHandler* event_handler);
    virtual void unregister_event_handlers();
    
    // Initialize component resources - called when OpenGL context is available
    virtual bool initialize();
    void render();
    void set_position(const float x, const float y);
    void get_position(float& x, float& y) const;
    void set_dimensions(const float width, const float height);
    void get_dimensions(float& width, float& height) const;
    
    // Set the render context for this component
    void set_render_context(const RenderContext& render_context);
    
    // Legacy method - use set_render_context instead
    void set_display_id(unsigned int id);

    // Adding a child component (takes ownership)
    void add_child(GraphicsComponent* child);
    
    // Removing a child component (releases ownership)
    void remove_child(GraphicsComponent* child);
    
    // Gets a pointer to a child (does not transfer ownership)
    GraphicsComponent* get_child(size_t index) const;
    
    // Gets the number of children
    size_t get_child_count() const;
    
    // Debug outline display
    void set_show_outline(bool show) { m_show_outline = show; }
    bool get_show_outline() const { return m_show_outline; }
    void set_outline_color(float r, float g, float b, float a);
    
    // Static method to toggle outline display for all components
    static void set_global_outline(bool show) { s_global_outline = show; }
    
protected:
    // New methods for viewport-based rendering
    void begin_local_rendering();
    void end_local_rendering();
    
    virtual void render_content(); // Components should override this instead of render()

    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_width = 0.0f;
    float m_height = 0.0f;
    
    // Render context for this component
    RenderContext m_render_context;

    // Event handler entries
    EventHandler* m_event_handler = nullptr;
    std::vector<std::shared_ptr<EventHandlerEntry>> m_event_handler_entries;
    bool m_event_handlers_registered = false; // Flag to prevent double registration
    
    // Component initialization tracking
    bool m_initialized = false;
    
    // Child components (owned by this component)
    std::vector<std::unique_ptr<GraphicsComponent>> m_children;
    
    // Debug outline settings
    bool m_show_outline = false;
    float m_outline_color[4] = {1.0f, 0.0f, 1.0f, 1.0f}; // Default to magenta
    
    // Draw component outline for debugging
    virtual void draw_outline();
    
    // Static variable for global outline control
    static bool s_global_outline;
    
    // Saved viewport and scissor state for local rendering
    GLint m_saved_viewport[4] = {0, 0, 0, 0};
    GLint m_saved_scissor_box[4] = {0, 0, 0, 0};
    GLboolean m_saved_scissor_test = GL_FALSE;
};