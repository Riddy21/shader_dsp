#pragma once
#ifndef GRAPHICS_DISPLAY_H
#define GRAPHICS_DISPLAY_H

#include <string>
#include <GL/glew.h>
#include <unordered_map>
#include <memory>
#include <vector>

#include "engine/renderable_item.h"
#include "engine/event_handler.h"
#include "graphics_core/graphics_view.h"
#include "graphics_core/graphics_component.h"

class GraphicsDisplay : public IRenderableEntity {
public:
    friend class GraphicsView; // Allow GraphicsView to access private members

    GraphicsDisplay(
        unsigned int width = 800, 
        unsigned int height = 600, 
        const std::string& title = "Graphics Display", 
        unsigned int refresh_rate = 60,
        EventHandler& event_handler = EventHandler::get_instance()
    );
    ~GraphicsDisplay();

    GraphicsDisplay(const GraphicsDisplay&) = delete;
    void operator=(const GraphicsDisplay&) = delete;

    // IEventLoopItem interface
    bool is_ready() override;
    void render() override;

    // View management
    void add_view(const std::string& name, GraphicsView* view);
    void change_view(const std::string& name);
    
private:
    unsigned int m_width;
    unsigned int m_height;
    std::string m_title;
    unsigned int m_refresh_rate; // Refresh rate in FPS

    Uint32 m_last_render_time = 0; // for FPS tracking

    GLuint m_VAO, m_VBO;

    std::unordered_map<std::string, std::unique_ptr<GraphicsView>> m_views;
    std::vector<std::unique_ptr<GraphicsComponent>> m_components; // TODO: Implement components management later
    GraphicsView* m_current_view = nullptr;
    EventHandler& m_event_handler; // REference cannot be re-assigned
};

#endif // GRAPHICS_DISPLAY_H
