#include <algorithm>

#include "graphics_core/graphics_view.h"
#include "graphics/graphics_display.h"

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

void GraphicsView::initialize(EventHandler& event_handler, unsigned int display_id) {
    set_display_id(display_id);

    set_event_handler(event_handler);
}

void GraphicsView::set_display_id(unsigned int id) {
    m_display_id = id;

    // Update all components with the new display ID
    for (auto& component :m_components) {
        component->set_display_id(id);
    }
}


void GraphicsView::set_event_handler(EventHandler& event_handler) {
    // If we already have an event handler, unregister from it first
    if (m_event_handler && m_event_handlers_registered) {
        unregister_event_handler(*m_event_handler);
    }

    m_event_handler = &event_handler;

    // Only register if not already registered
    register_event_handler(*m_event_handler);
}

void GraphicsView::register_event_handler(EventHandler& event_handler) {
    if (m_event_handlers_registered) return;
    // Register event handlers for each button component
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