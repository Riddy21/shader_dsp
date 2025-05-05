#pragma once
#ifndef GRAPHICS_DISPLAY_H
#define GRAPHICS_DISPLAY_H

#include <string>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "engine/event_loop.h"

class GraphicsDisplay : public IEventLoopItem {
public:
    GraphicsDisplay(unsigned int width = 800, unsigned int height = 600, const std::string& title = "Graphics Display");
    ~GraphicsDisplay();

    GraphicsDisplay(const GraphicsDisplay&) = delete;
    void operator=(const GraphicsDisplay&) = delete;

    // IEventLoopItem interface
    bool is_ready() override;
    void handle_event(const SDL_Event& event) override;
    void render() override;

private:
    unsigned int m_width;
    unsigned int m_height;
    std::string m_title;

    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;

    GLuint m_VAO, m_VBO;
};

#endif // GRAPHICS_DISPLAY_H
