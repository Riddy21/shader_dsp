#pragma once
#ifndef GRAPHICS_DISPLAY_H
#define GRAPHICS_DISPLAY_H

#include <string>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <unordered_map>
#include "engine/event_loop.h"
#include "graphics_core/graphics_view.h"

class GraphicsDisplay : public IEventLoopItem {
public:
    GraphicsDisplay(
        unsigned int width = 800, 
        unsigned int height = 600, 
        const std::string& title = "Graphics Display", 
        unsigned int refresh_rate = 60
    );
    ~GraphicsDisplay();

    GraphicsDisplay(const GraphicsDisplay&) = delete;
    void operator=(const GraphicsDisplay&) = delete;

    // IEventLoopItem interface
    bool is_ready() override;
    bool handle_event(const SDL_Event& event) override;
    void render() override;
    void present() override;

    // View management
    void register_view(const std::string& name, GraphicsView* view);
    void change_view(const std::string& name);

private:
    unsigned int m_width;
    unsigned int m_height;
    std::string m_title;
    unsigned int m_refresh_rate; // Refresh rate in FPS

    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;

    GLuint m_VAO, m_VBO;

    std::unordered_map<std::string, std::unique_ptr<GraphicsView>> m_views;
    std::vector<std::unique_ptr<GraphicsComponent>> m_components;
    GraphicsView* m_current_view = nullptr;
};

#endif // GRAPHICS_DISPLAY_H
