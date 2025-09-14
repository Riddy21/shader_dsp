#include <algorithm>

#include "graphics_core/graphics_view.h"
#include "engine/event_handler.h"

GraphicsView::GraphicsView() {
}

GraphicsView::~GraphicsView() {
    on_exit();
}

void GraphicsView::render() {
    for (auto& component : m_components) {
        component->render();
    }
}

void GraphicsView::on_enter() {
    register_event_handler(*m_event_handler);
}

void GraphicsView::on_exit() {
    unregister_event_handler(*m_event_handler);
}

void GraphicsView::add_component(GraphicsComponent* component) {
    m_components.push_back(std::unique_ptr<GraphicsComponent>(component));
    // If event handlers are already registered, register for the new component as well
    if (m_event_handlers_registered && m_event_handler) {
        component->register_event_handlers(m_event_handler);
    }
    
    // Set the render context for the component
    component->set_render_context(m_render_context);
}

void GraphicsView::remove_component(GraphicsComponent* component) {
    // If event handlers are registered, unregister for this component before removal
    if (m_event_handlers_registered) {
        component->unregister_event_handlers();
    }
    auto it = std::remove_if(m_components.begin(), m_components.end(),
                             [component](const std::unique_ptr<GraphicsComponent>& ptr) {
                                 return ptr.get() == component;
                             });
    m_components.erase(it, m_components.end());
}

void GraphicsView::initialize(EventHandler& event_handler, const RenderContext& render_context) {
    set_render_context(render_context);
    set_event_handler(event_handler);
    
    // Initialize all components now that we have a valid OpenGL context
    for (auto& component : m_components) {
        component->initialize();
    }
}

void GraphicsView::initialize(EventHandler& event_handler, unsigned int display_id) {
    // Create a temporary context with just the window_id for backward compatibility
    RenderContext ctx;
    ctx.window_id = display_id;
    
    initialize(event_handler, ctx);
}

void GraphicsView::set_render_context(const RenderContext& render_context) {
    m_render_context = render_context;

    // Update all components with the new render context
    for (auto& component : m_components) {
        component->set_render_context(render_context);
    }
}

void GraphicsView::set_display_id(unsigned int id) {
    // Create a temporary context with just the window_id for backward compatibility
    RenderContext ctx;
    ctx.window_id = id;
    
    set_render_context(ctx);
}


void GraphicsView::set_event_handler(EventHandler& event_handler) {
    // If we already have an event handler, unregister from it first
    if (m_event_handler && m_event_handlers_registered) {
        unregister_event_handler(*m_event_handler);
    }

    m_event_handler = &event_handler;
}

void GraphicsView::register_event_handler(EventHandler& event_handler) {
    if (m_event_handlers_registered) return;
    
    // First set the render context for all components
    for (auto & component : m_components) {
        component->set_render_context(m_render_context);
    }
    
    // Then register event handlers for each component
    for (auto & component : m_components) {
        component->register_event_handlers(&event_handler);
    }
    
    m_event_handlers_registered = true;
}

void GraphicsView::unregister_event_handler(EventHandler& event_handler) {
    if (!m_event_handlers_registered) return;
    // Unregister event handlers for each button component
    for (auto & component : m_components) {
        component->unregister_event_handlers();
    }
    m_event_handlers_registered = false;
}