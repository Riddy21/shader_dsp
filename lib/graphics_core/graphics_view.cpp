#include <algorithm>

#include "graphics_core/graphics_view.h"

void GraphicsView::on_event(const SDL_Event& event) {
    for (auto& component : m_components) {
        component->handle_event(event);
    }
}

void GraphicsView::on_render() {
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
