#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include <string>

#include "engine/event_handler.h"
#include "engine/renderable_entity.h"

// FIXME: Think about whether an event handler and render context should be passed for every component
class GraphicsComponent {
public:
    // Positioning mode: how x,y coordinates are interpreted
    enum class PositionMode {
        TOP_LEFT,     // x,y is top-left corner (default)
        CENTER,       // x,y is center point
        CENTER_BOTTOM,// x,y is center of bottom edge
        CENTER_TOP,   // x,y is center of top edge
        TOP_RIGHT,    // x,y is top-right corner
        BOTTOM_LEFT,  // x,y is bottom-left corner
        BOTTOM_RIGHT  // x,y is bottom-right corner
    };
    
    // Constructor with position, dimensions, event handler, and render context
    GraphicsComponent(
        const float x = 0.0f, 
        const float y = 0.0f, 
        const float width = 0.0f, 
        const float height = 0.0f,
        PositionMode position_mode = PositionMode::TOP_LEFT,
        EventHandler* event_handler = nullptr,
        const RenderContext& render_context = RenderContext()
    );
    
    virtual ~GraphicsComponent();

    // Event handler registration methods
    virtual void register_event_handlers(EventHandler* event_handler);
    virtual void unregister_event_handlers();
    
    // Initialize component resources - called when OpenGL context is available
    virtual bool initialize();
    virtual void render();
    void set_position(const float x, const float y);
    void get_position(float& x, float& y) const;
    void set_dimensions(const float width, const float height);
    void get_dimensions(float& width, float& height) const;
    
    // Position mode getters/setters
    void set_position_mode(PositionMode mode);
    PositionMode get_position_mode() const { return m_position_mode; }
    
    // Get corner position (always returns top-left corner regardless of mode)
    void get_corner_position(float& x, float& y) const;
    
    // Get center position
    void get_center_position(float& x, float& y) const;
    
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
    
    // Post-processing support
    // When enabled, the component and its children are rendered to an FBO first,
    // then the render_post_process() method is called to draw the result to the screen.
    // By default, post-processing is disabled.
    void set_post_processing_enabled(bool enabled);
    bool is_post_processing_enabled() const { return m_post_processing_enabled; }
    
    // Set a custom post-processing fragment shader.
    // The shader should have:
    // - Input: in vec2 TexCoord (texture coordinates)
    // - Uniform: uniform sampler2D uTexture (the rendered FBO texture)
    // - Output: out vec4 FragColor
    // If no custom shader is set, a default pass-through shader is used.
    // This should be called before initialize() or before enabling post-processing.
    void set_post_process_fragment_shader(const std::string& fragment_shader_src);

protected:
    // New methods for viewport-based rendering
    void begin_local_rendering();
    void end_local_rendering();
    
    virtual void render_content(); // Components should override this instead of render()

    // Renders the FBO texture to the screen. Derived classes can override this to apply effects.
    // The default implementation draws a simple textured quad.
    virtual void render_post_process();

    float m_x = 0.0f;  // Always stored as top-left corner (normalized coordinates)
    float m_y = 0.0f;  // Always stored as top-left corner (normalized coordinates)
    float m_width = 0.0f;
    float m_height = 0.0f;
    PositionMode m_position_mode = PositionMode::TOP_LEFT;  // How x,y is interpreted
    
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

    // Post-processing resources
    bool m_post_processing_enabled = false;
    std::string m_custom_post_frag_shader; // Custom fragment shader source (empty = use default)
    GLuint m_fbo = 0;
    GLuint m_texture = 0;
    GLuint m_rbo = 0; // For depth/stencil if needed
    int m_fbo_width = 0;
    int m_fbo_height = 0;
    
    // For rendering the FBO texture to screen (default implementation)
    GLuint m_post_vao = 0;
    GLuint m_post_vbo = 0;
    GLuint m_post_program = 0;
    
    void prepare_fbo();
    void cleanup_fbo();
    void initialize_post_process_resources();
    void compile_post_process_shader();
    
    // Global offsets for viewport calculation (used when rendering to FBO)
    static int s_viewport_offset_x;
    static int s_viewport_offset_y;
};
