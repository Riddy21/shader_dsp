#include <iostream>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "audio_render_stage/audio_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"

#include "audio_core/audio_render_graph.h"

AudioRenderGraph::AudioRenderGraph(const std::vector<AudioRenderStage *> inputs) {
    if (inputs.size() == 0) {
        throw std::invalid_argument("No input render stages.");
    }

    // Save inputs
    for (auto input : inputs) {
        m_inputs.push_back(input->gid);
    }

    // Construct the graph
    if (!construct_graph(inputs)) {
        throw std::runtime_error("Failed to construct graph from render stages");
    }

    printf("%d render stages parsed\n", m_render_stages_map.size());

    // Construct the render order
    auto visited = std::unordered_set<GID>();
    if (!construct_render_order(m_outputs[0], visited)) {
        throw std::runtime_error("Failed to construct render order.");
    }

    printf("Render stage order: ");
    for (auto gid : m_render_order) {
        printf("%d ", gid);
    }
    printf("\n");

}

AudioRenderStage * AudioRenderGraph::find_render_stage(GID gid) {
    if (m_render_stages_map.find(gid) != m_render_stages_map.end()) {
        return m_render_stages_map[gid].get();
    }
    return nullptr;
}

AudioRenderGraph::~AudioRenderGraph() {
    m_outputs.clear();
    m_inputs.clear();
    m_graph.clear();
    m_render_order.clear();
    m_render_stages_map.clear();
}

bool AudioRenderGraph::construct_graph(const std::vector<AudioRenderStage *> & inputs) {
    // Perform dfs on each input
    auto visited = std::unordered_set<GID>();
    for (auto input : inputs) {
        if (!dfs_construct_graph(input, visited)) {
            std::cerr << "Error: Graph is not connected." << std::endl;
            return false;
        }
    }

    return true;
}

bool AudioRenderGraph::dfs_construct_graph(AudioRenderStage * node, std::unordered_set<GID> & visited) {
    // Check if the node is already in the graph
    if (visited.find(node->gid) != visited.end()) {
        return true;
    }

    if (m_render_stages_map.find(node->gid) == m_render_stages_map.end()){
        m_render_stages_map[node->gid] = std::unique_ptr<AudioRenderStage>(node);
    }

    // Add the node to the visited list
    visited.insert(node->gid);

    // Find linked render stages via parameters
    auto linked_stages = find_linked_render_stages(node);

    // Find the output node
    if (linked_stages.size() == 0) {
        // Check if it is an output node
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
        if (!dfs_construct_graph(linked_stage, visited)) {
            return false;
        }
        // Add the edge to the graph backwards
        m_graph[linked_stage->gid].insert(node->gid);
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

bool AudioRenderGraph::construct_render_order(GID node, std::unordered_set<GID> & visited) {
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
    // Check that outputs are Final render stage
    for (auto & gid : m_outputs){
        auto & node = m_render_stages_map[gid];
        if (dynamic_cast<AudioFinalRenderStage*>(node.get()) == nullptr) {
            std::cerr << "Error: Did not end in output node " << node->gid << std::endl;
            return false;
        }
    }

    // Initialize the render stages
    for (auto & [gid, render_stage] : m_render_stages_map) {
        if (!render_stage->initialize_shader_stage()) {
            std::cerr << "Error: Failed to initialize render stage " << gid << std::endl;
            return false;
        }
    }

    if (!bind_render_stages()) {
        std::cerr << "Failed to bind render graph." << std::endl;
        return false;
    }


    return true;
}

bool AudioRenderGraph::bind_render_stages() {
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

bool AudioRenderGraph::link_render_stages(AudioRenderStage * input, AudioRenderStage * output) {
    auto * output_parameter = input->find_parameter("output_audio_texture");

    if (output_parameter->is_connected()) {
        printf("Output parameter %s in render stage %d is already linked\n", output_parameter->name.c_str(), input->gid);
        return false;
    }

    if (!output_parameter){
        printf("Could not find output parameter %s in render stage %d\n", output_parameter->name.c_str(), input->gid);
        return false;
    }

    if (auto * multi_join_out = dynamic_cast<AudioMultitrackJoinRenderStage*>(output)){
        auto * free_param = multi_join_out->get_free_stream_parameter();
        output_parameter->link(free_param);
        // TODO: Add a fork render stage
    } else {
        // Find parameters to join
        auto * stream_param = output->find_parameter("stream_audio_texture");
        if (!stream_param){
            printf("Could not find output parameter %s in render stage %d\n", "output_audio_texture", input->gid);
            return false;
        }
        output_parameter->link(stream_param);
        // TODO: Consider making this data driven
    }
    return true;
}

bool AudioRenderGraph::unlink_render_stages(AudioRenderStage * input, AudioRenderStage * output) {
    auto * output_parameter = input->find_parameter("output_audio_texture");

    // Find the linked render stage name
    auto * stream_parameter = output_parameter->get_linked_parameter();
    if (stream_parameter->get_linked_render_stage() != output) {
        printf("Output render stage %d specified is not linked to the render stage %d\n", output->gid, input->gid);
        return false;
    }
            
    // Release from used if multi track join
    if (auto * multi_join_out = dynamic_cast<AudioMultitrackJoinRenderStage*>(output)) {
        multi_join_out->release_stream_parameter(stream_parameter);
    }

    // Unlink input audio texture
    output_parameter->unlink();

    return true;
}

// TODO: Implement inline flag
bool AudioRenderGraph::insert_render_stage_behind(GID front, AudioRenderStage * render_stage) {
    // Make sure front exists
    if (m_render_stages_map.find(front) == m_render_stages_map.end()) {
        printf("Did not find render stage %d in graph\n", front);
        return false;
    }
    // Make sure render stage does not exist
    if (m_render_stages_map.find(render_stage->gid) != m_render_stages_map.end()) {
        printf("Render stage %d is already in graph\n", render_stage->gid);
        return false;
    }

    // Link it to the render stage
    link_render_stages(m_render_stages_map[front].get(), render_stage);

    // Clear the render_map
    m_graph.clear();
    m_render_order.clear();
    m_outputs.clear();

    // Re-generate graph
    std::vector<AudioRenderStage*> inputs;
    for (auto & gid : m_inputs) {
        inputs.push_back(m_render_stages_map[gid].get());
    }

    // Construct the graph
    if (!construct_graph(inputs)) {
        throw std::runtime_error("Failed to construct graph from render stages");
    }

    // Re-generate the render order and ouptuts
    auto visited = std::unordered_set<GID>();
    if (!construct_render_order(m_outputs[0], visited)) {
        throw std::runtime_error("Failed to construct render order.");
    }

    return true;
}