#include "audio_synthesizer/audio_module.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"

void AudioModuleManager::add_module(std::shared_ptr<AudioModule> module) {
    if (module == nullptr) {
        throw std::invalid_argument("Cannot add a null module");
    }

    // Add the module to the list
    m_module_index[module->name()] = m_modules.size();
    m_modules.push_back(module);

    // Add the render stage to the render graph at the root
    for (auto & render_stage : module->m_render_stages) {
        m_render_graph.insert_render_stage_infront(m_graph_root, render_stage);
    }
}

std::shared_ptr<AudioModule> AudioModuleManager::replace_module(const std::string & old_module_name, std::shared_ptr<AudioModule> new_module) {
    if (new_module == nullptr) {
        throw std::invalid_argument("Cannot replace a null module");
    }

    // Find the index of the old module
    auto it = m_module_index.find(old_module_name);
    if (it == m_module_index.end()) {
        throw std::runtime_error("Module not found: " + old_module_name);
    }
    unsigned int index = it->second;
    auto old_module = m_modules[index];

    // Find the root stage before the old stage
    unsigned int root_stage_gid;
    if (index == m_modules.size() - 1) {
        root_stage_gid = m_graph_root;
    } else {
        root_stage_gid = m_modules[index + 1]->m_render_stages.front()->gid;
    }

    // TODO: Add functionality to replace subgraphs in render graph
    for (auto & render_stage : old_module->m_render_stages) {
        m_render_graph.remove_render_stage(render_stage->gid);
    }
    for (auto & render_stage : new_module->m_render_stages) {
        m_render_graph.insert_render_stage_infront(m_graph_root, render_stage);
    }

    m_modules[index] = new_module;
    m_module_index.erase(old_module_name);
    m_module_index[new_module->name()] = index;

    return old_module;
}

std::vector<std::string> AudioModuleManager::get_module_names() const {
    std::vector<std::string> names;
    for (const auto & module : m_modules) {
        names.push_back(module->name());
    }
    return names;
}

std::vector<AudioControlBase *> AudioModuleManager::get_all_controls() const {
    std::vector<AudioControlBase *> all_controls;
    for (const auto & module : m_modules) {
        all_controls.insert(all_controls.end(), module->m_controls.begin(), module->m_controls.end());
    }
    return all_controls;
}

AudioModule::AudioModule(const std::string& name,
                         const unsigned int buffer_size, 
                         const unsigned int sample_rate, 
                         const unsigned int num_channels)
    : m_name(name),
      m_buffer_size(buffer_size), 
      m_sample_rate(sample_rate), 
      m_num_channels(num_channels)
{}

AudioModule::AudioModule(const std::string& name,
                         const std::vector<std::shared_ptr<AudioRenderStage>>& render_stages)
    : m_name(name),
      m_buffer_size(render_stages.empty() ? 0 : render_stages.front()->frames_per_buffer),
      m_sample_rate(render_stages.empty() ? 0 : render_stages.front()->sample_rate),
      m_num_channels(render_stages.empty() ? 0 : render_stages.front()->num_channels),
      m_render_stages(render_stages)
{
    if (render_stages.empty()) {
        throw std::invalid_argument("Render stages cannot be empty");
    }
    for (const auto & stage : render_stages) {
        if (stage->frames_per_buffer != this->m_buffer_size ||
            stage->sample_rate != this->m_sample_rate ||
            stage->num_channels != this->m_num_channels) {
            throw std::invalid_argument("All render stages must have the same buffer size, sample rate, and number of channels");
        }
    }
    m_controls.clear();
    for (const auto & stage : m_render_stages) {
        for (auto & control : stage->get_controls()) {
            m_controls.push_back(control);
        }
    }
}

AudioEffectModule::AudioEffectModule(const std::string& name,
                                     const std::vector<std::shared_ptr<AudioEffectRenderStage>>& render_stages)
    : AudioModule(name, std::vector<std::shared_ptr<AudioRenderStage>>(render_stages.begin(), render_stages.end()))
{}
AudioEffectModule::AudioEffectModule(const std::string& name,
                                     const std::vector<AudioEffectRenderStage*>& render_stages)
    : AudioModule(name, std::vector<std::shared_ptr<AudioRenderStage>>(render_stages.begin(), render_stages.end()))
{}

AudioVoiceModule::AudioVoiceModule(const std::string& name, 
                                   AudioGeneratorRenderStage * generator_render_stages)
    : AudioModule(name, (AudioRenderStage * )generator_render_stages),
      m_generator_render_stage(generator_render_stages)
{
}


void AudioVoiceModule::play_note(float tone, float gain) {
    m_generator_render_stage->play_note(tone, gain);
}
void AudioVoiceModule::stop_note(float tone) {
    m_generator_render_stage->stop_note(tone);
}