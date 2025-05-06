#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

#include <graphics_core/graphics_component.h>

class GraphicsView {
public:
    virtual ~GraphicsView() = default;
    virtual void on_event(const SDL_Event& event);
    virtual void on_render();
    virtual void on_enter() {}
    virtual void on_exit() {}

    void add_component(GraphicsComponent* component);
    void remove_component(GraphicsComponent* component);

private:

    std::vector<std::unique_ptr<GraphicsComponent>> m_components;
};
