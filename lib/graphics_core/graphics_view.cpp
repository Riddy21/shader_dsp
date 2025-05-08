#include <algorithm>

#include "graphics_core/graphics_view.h"

bool GraphicsView::handle_event(const SDL_Event& event) {
    bool state_changed = false;

    for (auto& component : m_components) {
        if (component->handle_event(event)) {
            state_changed = true;
        }
    }

    return state_changed;
}

void GraphicsView::render() {
    for (auto& component : m_components) {
        component->render();
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
