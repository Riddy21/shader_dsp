#ifndef GRAPH_COMPONENT_H
#define GRAPH_COMPONENT_H

#include <memory>
#include <string>
#include <vector>
#include <SDL2/SDL.h>

#include "utilities/shader_program.h"
#include "engine/event_handler.h"
#include "graphics_core/graphics_component.h"

class GraphComponent : public GraphicsComponent {
public:
    GraphComponent(const float x, const float y, const float width, const float height, 
                  const std::vector<float>& data, const bool is_dynamic = true,
                  PositionMode position_mode = PositionMode::TOP_LEFT);
    ~GraphComponent() override;

    void set_data(const std::vector<float>& data);
    
protected:
    bool initialize() override;
    void render_content() override;

private:
    bool m_is_dynamic = true;
    const std::vector<float> * m_data;

    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
};

#endif // GRAPH_COMPONENT_H
