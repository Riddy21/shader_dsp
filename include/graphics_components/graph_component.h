#ifndef GRAPH_COMPONENT_H
#define GRAPH_COMPONENT_H

#include <memory>
#include <string>
#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "utilities/shader_program.h"
#include "graphics_core/graphics_component.h"

class GraphComponent : public GraphicsComponent {
public:
    GraphComponent(float x, float y, float width, float height, const float* data, size_t size);
    ~GraphComponent() override;

    void set_data(const float* data, size_t size);
    void handle_event(const SDL_Event& event) override;
    void render() override;

private:
    const float* m_data;
    size_t m_data_size;

    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
};

#endif // GRAPH_COMPONENT_H
