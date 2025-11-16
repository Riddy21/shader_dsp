#pragma once
#ifndef AUDIO_RENDER_STAGE_PLUGIN_H
#define AUDIO_RENDER_STAGE_PLUGIN_H

#include <vector>
#include <string>
#include <GLES3/gl3.h>
#include <cstdio>
#include <iostream>

// Forward declaration
class AudioParameter;

class AudioRenderStagePlugin {
public:
    virtual ~AudioRenderStagePlugin() = default;

    /**
     * @brief Get the plugin name (used for parameterizing variable and function names)
     * @return Plugin name string
     */
    virtual std::string get_plugin_name() const = 0;

protected:
    /**
     * @brief Replace plugin placeholder in shader source
     * Replaces {PLUGIN_SUFFIX} with either empty string (if plugin_name is empty) or "_plugin_name"
     * @param source The shader source string to process
     * @param plugin_name The plugin name to use for replacement
     * @return Processed shader source with placeholders replaced
     */
    static std::string replace_plugin_placeholder(const std::string& source, const std::string& plugin_name) {
        std::string result = source;
        std::string placeholder = "{PLUGIN_SUFFIX}";
        std::string replacement = plugin_name.empty() ? "" : ("_" + plugin_name);
        
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
        
        return result;
    }

    /**
     * @brief Generate a parameterized name for a parameter
     * If plugin_name is empty, returns base_name unchanged.
     * Otherwise, returns "base_name_plugin_name"
     * @param base_name The base parameter name
     * @param plugin_name The plugin name to append
     * @return Parameterized name string
     */
    static std::string make_parameterized_name(const std::string& base_name, const std::string& plugin_name) {
        if (plugin_name.empty()) {
            return base_name;
        }
        return base_name + "_" + plugin_name;
    }

public:

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
     * @brief Get processed fragment shader source for a given import path
     * This method reads the shader file and applies plugin-specific replacements
     * Default implementation automatically replaces {PLUGIN_SUFFIX} placeholders
     * @param import_path Path to the shader file
     * @return Processed shader source string with plugin-specific replacements applied
     */
    virtual std::string get_processed_fragment_shader_source(const std::string& import_path) const {
        // Read the shader file
        FILE* file = fopen(import_path.c_str(), "r");
        if (!file) {
            std::cerr << "Error: Failed to open file " << import_path << std::endl;
            return "";
        }
        
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char* source = new char[length + 1];
        fread(source, 1, length, file);
        source[length] = '\0';
        
        fclose(file);
        
        std::string result(source);
        delete[] source;
        
        // Apply plugin placeholder replacement automatically
        return replace_plugin_placeholder(result, get_plugin_name());
    }

    /**
     * @brief Get processed vertex shader source for a given import path
     * This method reads the shader file and applies plugin-specific replacements
     * Default implementation automatically replaces {PLUGIN_SUFFIX} placeholders
     * @param import_path Path to the shader file
     * @return Processed shader source string with plugin-specific replacements applied
     */
    virtual std::string get_processed_vertex_shader_source(const std::string& import_path) const {
        // Read the shader file
        FILE* file = fopen(import_path.c_str(), "r");
        if (!file) {
            std::cerr << "Error: Failed to open file " << import_path << std::endl;
            return "";
        }
        
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char* source = new char[length + 1];
        fread(source, 1, length, file);
        source[length] = '\0';
        
        fclose(file);
        
        std::string result(source);
        delete[] source;
        
        // Apply plugin placeholder replacement automatically
        return replace_plugin_placeholder(result, get_plugin_name());
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

