#include <algorithm>

#include "graphics_core/graphics_view.h"

#include "engine/event_handler.h"

GraphicsView::GraphicsView()
    : m_event_handler(nullptr)
{
}

GraphicsView::~GraphicsView() {
    // Make sure all event handlers are unregistered
    if (m_event_handler) {
        unregister_event_handler(m_event_handler);
    }
}

void GraphicsView::render() {
    for (auto& component : m_components) {
        component->render();
    }
}

void GraphicsView::on_enter() {
    // Register view-specific event handlers when the view becomes active
    if (m_event_handler) {
        register_event_handler(m_event_handler);
    }
}

void GraphicsView::on_exit() {
    // Unregister view-specific event handlers when the view becomes inactive
    if (m_event_handler) {
        unregister_event_handler(m_event_handler);
    }
}

void GraphicsView::add_component(GraphicsComponent* component) {
    std::unique_ptr<GraphicsComponent> ptr(component);
    m_components.push_back(std::move(ptr));
}

void GraphicsView::remove_component(GraphicsComponent* component) {
    auto it = std::remove_if(m_components.begin(), m_components.end(),
                             [component](const std::unique_ptr<GraphicsComponent>& ptr) {
                                 return ptr.get() == component;
                             });
    m_components.erase(it, m_components.end());
}

void GraphicsView::set_event_handler(EventHandler* event_handler) {
    // If we already have an event handler, unregister from it first
    if (m_event_handler) {
        unregister_event_handler(m_event_handler);
    }
    
    m_event_handler = event_handler;
    
    // Register with the new event handler if we're already active
    if (m_event_handler) {
        register_event_handler(m_event_handler);
    }
}

void GraphicsView::set_parent_display(IRenderableEntity* parent) {
    m_parent_display = parent;

    for (auto& component : m_components) {
        component->set_display_context(m_parent_display);
    }
}

void GraphicsView::register_event_handler(EventHandler* event_handler) {
}

void GraphicsView::unregister_event_handler(EventHandler* event_handler) {
    // Clear the list of registered entries
    m_registered_entries.clear();
}