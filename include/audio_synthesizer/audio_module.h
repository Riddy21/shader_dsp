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
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"

class AudioModule;

class AudioModuleManager {
public:
    AudioModuleManager(AudioRenderGraph & render_graph, const unsigned int graph_root, AudioRenderer & audio_renderer = AudioRenderer::get_instance());

    void add_module(std::shared_ptr<AudioModule> module);

    std::shared_ptr<AudioModule> replace_module(const std::string & old_module_name, std::shared_ptr<AudioModule> new_module);

    std::vector<std::string> get_module_names() const;

private:
    std::vector<std::shared_ptr<AudioModule>> m_modules; // List of audio modules in that order in the render graph
    std::unordered_map<std::string, unsigned int> m_module_index; // Map of module names to modules
    const std::vector<std::string> m_control_path = {"current"};

    AudioRenderGraph & m_render_graph; // Reference to the render graph for this module manager
    const unsigned int m_graph_root; // The root of the render graph where modules are added
    AudioRenderer & m_audio_renderer; // Reference to the audio renderer for this module manager
};

class AudioModule {
public:
    friend class AudioModuleManager;    
    virtual ~AudioModule() = default;
    std::string name() const { return m_name; }
    std::string type() const { return m_type; }

protected:
    AudioModule(const std::string& name,
                const std::string& type,
                const unsigned int buffer_size, 
                const unsigned int sample_rate, 
                const unsigned int num_channels);

    AudioModule(const std::string& name,
                const std::string& type,
                const std::vector<std::shared_ptr<AudioRenderStage>>& render_stages);

    AudioModule(const std::string& name,
                const std::string& type,
                const std::vector<AudioRenderStage*>& render_stages)
        : AudioModule(name, type, std::vector<std::shared_ptr<AudioRenderStage>>(render_stages.begin(), render_stages.end())) {}

    AudioModule(const std::string& name,
                const std::string& type,
                const std::shared_ptr<AudioRenderStage> render_stage)
        : AudioModule(name, type, std::vector<std::shared_ptr<AudioRenderStage>>{render_stage}) {}

    AudioModule(const std::string& name,
                const std::string& type,
                AudioRenderStage* render_stage)
        : AudioModule(name, type, std::vector<std::shared_ptr<AudioRenderStage>>{std::shared_ptr<AudioRenderStage>(render_stage)}) {}

    std::vector<std::shared_ptr<AudioRenderStage>> m_render_stages;
    std::vector<std::shared_ptr<AudioControlBase>> m_controls;

    const std::string m_name;
    const std::string m_type;
    const unsigned int m_buffer_size;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;
};

// Effect module base class
class AudioEffectModule : public AudioModule {
public:
    AudioEffectModule(const std::string& name,
                      const std::vector<std::shared_ptr<AudioEffectRenderStage>>& render_stages);

    AudioEffectModule(const std::string& name,
                      const std::vector<AudioEffectRenderStage*>& render_stages);

    virtual ~AudioEffectModule() = default;
};

class AudioVoiceModule : public AudioModule {
public:
    AudioVoiceModule(const std::string& name,
                     AudioGeneratorRenderStage * generator_render_stages);

    virtual ~AudioVoiceModule() = default;

    void play_note(float tone, float gain);
    void stop_note(float tone);

protected:
    AudioGeneratorRenderStage * m_generator_render_stage; // The generator render stage for this voice module
};

#endif // AUDIO_MODULE_H