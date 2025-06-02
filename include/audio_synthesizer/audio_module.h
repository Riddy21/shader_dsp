#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H

#include <vector>
#include <memory>
#include <string>
#include <variant>
#include <unordered_map>
#include <functional>

#include "audio_core/audio_render_graph.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_control.h"

class AudioModule;

class AudioModuleManager {
public:
    AudioModuleManager(AudioRenderGraph & render_graph, const unsigned int graph_root, AudioRenderer & audio_renderer = AudioRenderer::get_instance())
        : m_render_graph(render_graph), m_graph_root(graph_root), m_audio_renderer(audio_renderer) {};

    void add_module(std::shared_ptr<AudioModule> module);

    std::shared_ptr<AudioModule> replace_module(const std::string & old_module_name, std::shared_ptr<AudioModule> new_module);

    std::vector<std::string> get_module_names() const;

private:
    std::vector<std::shared_ptr<AudioModule>> m_modules; // List of audio modules in that order in the render graph
    std::unordered_map<std::string, unsigned int> m_module_index; // Map of module names to modules

    AudioRenderGraph & m_render_graph; // Reference to the render graph for this module manager
    const unsigned int m_graph_root; // The root of the render graph where modules are added
    AudioRenderer & m_audio_renderer; // Reference to the audio renderer for this module manager
};

class AudioModule {
public:
    friend class AudioModuleManager;    
    virtual ~AudioModule() = default;
    virtual std::string name() const { return m_name; }

protected:
    AudioModule(const std::string& name,
                const unsigned int buffer_size, 
                const unsigned int sample_rate, 
                const unsigned int num_channels)
        : m_name(name),
          m_buffer_size(buffer_size), 
          m_sample_rate(sample_rate), 
          m_num_channels(num_channels) {}

    std::vector<std::shared_ptr<AudioRenderStage>> m_render_stages;
    std::vector<AudioControl<std::variant<float, int, bool>> * > m_controls;

    const std::string m_name;
    const unsigned int m_buffer_size;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;
};

// Effect module base class
class AudioEffectModule : public AudioModule {
public:
    AudioEffectModule(const std::string& name, 
                      const unsigned int buffer_size, 
                      const unsigned int sample_rate, 
                      const unsigned int num_channels);

    AudioEffectModule(const std::string& name,
                      const std::vector<std::shared_ptr<AudioRenderStage>>& render_stages);

    AudioEffectModule(const std::string& name,
                      const std::vector<AudioRenderStage*>& render_stages);
    

    virtual ~AudioEffectModule() = default;
};


class AudioVoiceModule : public AudioModule {
public:
    AudioVoiceModule(const std::string& name, 
                     const unsigned int buffer_size, 
                     const unsigned int sample_rate, 
                     const unsigned int num_channels);

    AudioVoiceModule(const std::string& name, 
                     const std::vector<std::shared_ptr<AudioRenderStage>>& render_stages);

    AudioVoiceModule(const std::string& name, 
                     const std::vector<AudioRenderStage*>& render_stages);

    virtual ~AudioVoiceModule() = default;

    virtual void play_note(float tone, float gain) {}
    virtual void stop_note(float tone) {}
};

#endif // AUDIO_MODULE_H