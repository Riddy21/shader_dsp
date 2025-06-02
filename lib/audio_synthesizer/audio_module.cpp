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
}