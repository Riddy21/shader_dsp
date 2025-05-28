#include "graphics_core/graphics_component.h"

// Constructor
GraphicsComponent::GraphicsComponent(
    const float x, 
    const float y, 
    const float width, 
    const float height
) : m_x(x), m_y(y), m_width(width), m_height(height) {}

// Virtual destructor is defaulted in header

void GraphicsComponent::render() {
    // Base implementation renders all children
    for (auto& child : m_children) {
        child->render();
    }
}

void GraphicsComponent::register_event_handlers(EventHandler* event_handler) {
    if (m_event_handlers_registered) return;
    m_event_handlers_registered = true;
    
    m_event_handler = event_handler;
    
    // Register event handlers for all children
    for (auto& child : m_children) {
        child->register_event_handlers(event_handler);
    }
}

void GraphicsComponent::unregister_event_handlers() {
    if (!m_event_handlers_registered) return;
    m_event_handlers_registered = false;
    
    // If we have an event handler, unregister all our entries
    if (m_event_handler) {
        for (auto* entry : m_event_handler_entries) {
            m_event_handler->unregister_entry(entry);
        }
        m_event_handler_entries.clear();
    }
    
    // Unregister event handlers for all children
    for (auto& child : m_children) {
        child->unregister_event_handlers();
    }
    
    m_event_handler = nullptr;
}

void GraphicsComponent::set_position(const float x, const float y) {
    float dx = x - m_x;
    float dy = y - m_y;
    m_x = x;
    m_y = y;
    
    // Update positions of all children
    for (auto& child : m_children) {
        float child_x, child_y;
        child->get_position(child_x, child_y);
        child->set_position(child_x + dx, child_y + dy);
    }
}

void GraphicsComponent::get_position(float& x, float& y) const {
    x = m_x;
    y = m_y;
}

void GraphicsComponent::set_dimensions(const float width, const float height) {
    m_width = width;
    m_height = height;
}

void GraphicsComponent::get_dimensions(float& width, float& height) const {
    width = m_width;
    height = m_height;
}

void GraphicsComponent::set_display_id(unsigned int id) {
    m_window_id = id;
    
    // Update window ID for all children
    for (auto& child : m_children) {
        child->set_display_id(id);
    }
}

void GraphicsComponent::add_child(GraphicsComponent* child) {
    // Take ownership of the child component
    m_children.push_back(std::unique_ptr<GraphicsComponent>(child));
    
    // Register event handlers for the child if we're already registered
    if (m_event_handlers_registered && m_event_handler) {
        child->register_event_handlers(m_event_handler);
    }
}

void GraphicsComponent::remove_child(GraphicsComponent* child) {
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (it->get() == child) {
            // Unregister event handlers for the child if we're registered
            if (m_event_handlers_registered) {
                (*it)->unregister_event_handlers();
            }
            
            // Release ownership and return the raw pointer
            it->release();
            m_children.erase(it);
            break;
        }
    }
}

GraphicsComponent* GraphicsComponent::get_child(size_t index) const {
    if (index < m_children.size()) {
        return m_children[index].get();
    }
    return nullptr;
}

size_t GraphicsComponent::get_child_count() const {
    return m_children.size();
}