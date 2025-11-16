#pragma once
#ifndef AUDIO_RENDER_STAGE_PLUGIN_H
#define AUDIO_RENDER_STAGE_PLUGIN_H

#include <vector>
#include <string>
#include <GLES3/gl3.h>

// Forward declaration
class AudioParameter;

class AudioRenderStagePlugin {
public:
    virtual ~AudioRenderStagePlugin() = default;

    /**
     * @brief Get fragment shader imports required by this plugin
     * @return Vector of shader file paths to import
     */
    virtual std::vector<std::string> get_fragment_shader_imports() const = 0;

    /**
     * @brief Get vertex shader imports required by this plugin
     * @return Vector of shader file paths to import (default: empty)
     */
    virtual std::vector<std::string> get_vertex_shader_imports() const {
        return {};
    }

    /**
     * @brief Create all parameters for this plugin
     * @param active_texture_count Reference to active texture count (will be incremented)
     * @param color_attachment_count Reference to color attachment count (will be incremented if needed)
     */
    virtual void create_parameters(GLuint& active_texture_count, GLuint& color_attachment_count) = 0;

    /**
     * @brief Get all parameters created by this plugin
     * @return Vector of parameter pointers
     */
    virtual std::vector<AudioParameter*> get_parameters() const = 0;
};

#endif // AUDIO_RENDER_STAGE_PLUGIN_H

