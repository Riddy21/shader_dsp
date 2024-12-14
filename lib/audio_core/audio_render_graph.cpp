#include <iostream>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <memory>

#include "audio_render_stage/audio_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"

#include "audio_core/audio_render_graph.h"

AudioRenderGraph::AudioRenderGraph(const std::vector<AudioRenderStage *> inputs) {
    if (inputs.size() == 0) {
        std::cerr << "Error: No input render stages." << std::endl;
        return;
    }

    // Construct the graph
    if (!construct_graph(inputs)) {
        std::cerr << "Error: Failed to construct graph." << std::endl;
        return;
    }

    printf("%d render stages parsed\n", m_render_stages_map.size());

    // Construct the render order
    auto visited = std::unordered_set<GID>();
    if (!construct_render_order(m_outputs[0], visited)) {
        std::cerr << "Error: Failed to construct render order." << std::endl;
        return;
    }

    printf("Render stage order: ");
    for (auto gid : m_render_order) {
        printf("%d ", gid);
    }
    printf("\n");

}

AudioRenderStage * AudioRenderGraph::find_render_stage(const GID gid) {
    if (m_render_stages_map.find(gid) != m_render_stages_map.end()) {
        return m_render_stages_map[gid].get();
    }
    return nullptr;
}

AudioRenderGraph::~AudioRenderGraph() {
    m_graph.clear();
    m_render_order.clear();
    m_render_stages_map.clear();
}

bool AudioRenderGraph::construct_graph(const std::vector<AudioRenderStage *> & inputs) {
    // Perform dfs on each input
    for (auto input : inputs) {
        if (!dfs_construct_graph(input)) {
            std::cerr << "Error: Graph is not connected." << std::endl;
            return false;
        }
    }

    return true;
}

bool AudioRenderGraph::dfs_construct_graph(AudioRenderStage * node) {
    // Check if the node is already in the graph
    if (m_render_stages_map.find(node->gid) != m_render_stages_map.end()) {
        return true;
    }

    // Add the node to the visited list
    m_render_stages_map[node->gid] = std::unique_ptr<AudioRenderStage>(node);

    // Find linked render stages via parameters
    auto linked_stages = find_linked_render_stages(node);

    // Find the output node
    if (linked_stages.size() == 0) {
        // Check if it is an output node
        if (dynamic_cast<AudioFinalRenderStage*>(node) == nullptr) {
            std::cerr << "Error: Did not end in output node " << node->gid << std::endl;
            return false;
        }
        // Add the output node to the output list
        m_outputs.push_back(node->gid);
        // TODO: Currently only supporting one output node
        if (m_outputs.size() > 1) {
            std::cerr << "Error: More than one output node." << std::endl;
            return false;
        }
    }

    for (auto linked_stage : linked_stages) {
        // Perform dfs on the linked stage
        if (!dfs_construct_graph(linked_stage)) {
            return false;
        }
        // Add the edge to the graph backwards
        m_graph[linked_stage->gid].push_back(node->gid);
    }

    return true;
}

std::vector<AudioRenderStage *> AudioRenderGraph::find_linked_render_stages(const AudioRenderStage * render_stage) {
    std::vector<AudioRenderStage *> linked_stages;
    
    // Find output parameters
    auto output_parameters = render_stage->get_output_parameters();
    for (auto output_parameter : output_parameters) {
        // Find the linked parameter
        auto linked_parameter = output_parameter->get_linked_parameter();
        if (linked_parameter) {
            // Find the linked render stage
            auto linked_render_stage = linked_parameter->get_linked_render_stage();
            if (linked_render_stage) {
                linked_stages.push_back(linked_render_stage);
            }
        }

    }

    return linked_stages;
}

bool AudioRenderGraph::construct_render_order(const GID node, std::unordered_set<GID> & visited) {
    // Check if the node is already in the render order
    if (std::find(m_render_order.begin(), m_render_order.end(), node) != m_render_order.end()) {
        return true;
    }

    // Add the node to the render order from the front
    m_render_order.insert(m_render_order.begin(), node);

    // Traverse the graph
    for (auto next_node : m_graph[node]) {
        if (!construct_render_order(next_node, visited)) {
            return false;
        }
    }

    return true;

}

bool AudioRenderGraph::initialize() {
    // Initialize the render stages
    for (auto & [gid, render_stage] : m_render_stages_map) {
        if (!render_stage->initialize_shader_stage()) {
            std::cerr << "Error: Failed to initialize render stage " << gid << std::endl;
            return false;
        }
    }

    // bind the render stages
    for (auto & [gid, render_stage] : m_render_stages_map) {
        if (!render_stage->bind_shader_stage()) {
            std::cerr << "Error: Failed to bind render stage " << gid << std::endl;
            return false;
        }
    }

    return true;
}

void AudioRenderGraph::render() {
    // Render the render stages in order
    for (auto gid : m_render_order) {
        m_render_stages_map[gid]->render_render_stage();
    }
}