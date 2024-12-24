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

AudioRenderGraph::AudioRenderGraph(AudioRenderStage * output) {
    // Construct the render order
    if (!construct_render_order(output)) {
        throw std::runtime_error("Failed to construct render order.");
    }

    printf("Render stage order: ");
    for (auto & gid : m_render_order) {
        printf("%d ", gid);
    }
    printf("\n");
}

AudioRenderGraph::AudioRenderGraph(std::vector<AudioRenderStage *> inputs) {
    // Traverse inputs to find ouptut
    std::unordered_set<GID> visited;

    std::vector<AudioRenderStage *> outputs;
    for (auto & node : inputs) {
        auto * output = from_input_to_output(node, visited);
        if (output != nullptr){
            outputs.push_back(output);
        }
    }

    // If there is more than one output then error
    if (outputs.size() != 1){
        throw std::runtime_error("Wrong number of render outputs found, 1 expected");
    }

    // Construct the render order
    if (!construct_render_order(outputs[0])) {
        throw std::runtime_error("Failed to construct render order.");
    }

    printf("Render stage order: ");
    for (auto & gid : m_render_order) {
        printf("%d ", gid);
    }
    printf("\n");
}

AudioRenderStage * AudioRenderGraph::from_input_to_output(AudioRenderStage * node, std::unordered_set<GID> & visited) {
    // Break cases
    if (node->m_connected_output_render_stages.size() == 0) {
        // Check if it is the ouput
        if (dynamic_cast<AudioFinalRenderStage*>(node) == nullptr) {
            throw std::runtime_error("Render graph did not end in Final render stage");
        }
        return node;
    }

    if (visited.find(node->gid) != visited.end()){
        return nullptr;
    }

    visited.insert(node->gid);

    std::vector<AudioRenderStage *> outputs;

    for (auto * next_node: node->m_connected_output_render_stages){
        auto * output = from_input_to_output(next_node, visited);
        if (output != nullptr){
            outputs.push_back(output);
        }
    }

    // If there is more than one output then error
    if (outputs.size() > 1){
        throw std::runtime_error("Wrong number of render outputs found, 1 expected");
    } else if (outputs.size() < 1) {
        return nullptr;
    }

    return outputs[0];
}

AudioRenderGraph::~AudioRenderGraph() {
    m_outputs.clear();
    m_inputs.clear();
    m_render_order.clear();
    m_render_stages_map.clear();
}

// Pull directly from render stage graph
bool AudioRenderGraph::construct_render_order(AudioRenderStage * node) {

    if (dynamic_cast<AudioFinalRenderStage*>(node) != nullptr) {
        m_outputs.push_back(node->gid);
    }

    // Check if the node exists
    if (node == nullptr) {
        return true;
    }

    // Check if the node is already in the render order
    if (std::find(m_render_order.begin(), m_render_order.end(), node->gid) != m_render_order.end()) {
        return true;
    }


    // Add the node to the render order from the front
    m_render_order.insert(m_render_order.begin(), node->gid);

    if (m_render_stages_map.find(node->gid) == m_render_stages_map.end()){
        m_render_stages_map[node->gid] = std::unique_ptr<AudioRenderStage>(node);
    }

    // If nodes aren't connected in streams, that means it's input node
    if (node->m_connected_stream_render_stages.size() == 0) {
        m_inputs.push_back(node->gid);
    }

    // Traverse the graph
    for (auto next_node : node->m_connected_stream_render_stages) {
        if (!construct_render_order(next_node)) {
            return false;
        }
    }

    return true;

}

bool AudioRenderGraph::initialize() {
    // Check that outputs are Final render stage
    for (auto gid : m_outputs){
        if (dynamic_cast<AudioFinalRenderStage*>(m_render_stages_map[gid].get()) == nullptr) {
            std::cerr << "Error: Did not end in output node " << gid << std::endl;
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
    for (auto & gid : m_render_order) {
        m_render_stages_map[gid]->render_render_stage();
    }
}

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
    
    // If there is multiple outputs
    if (int size = m_render_stages_map[front]->m_connected_output_render_stages.size() < 1) {
        printf("Can't add render stage after the final render stage.");
        return false;
    }

    if (int size = m_render_stages_map[front]->m_connected_output_render_stages.size() > 1) {
        printf("Can't add render stage after multitrack split render stage.");
        return false;
    }

    // Re-connect the render stages

    auto * front_render_stage = find_render_stage(front);
    auto * back_render_stage = *front_render_stage->m_connected_output_render_stages.begin();

    front_render_stage->disconnect_render_stage(back_render_stage);

    front_render_stage->connect_render_stage(render_stage);
    render_stage->connect_render_stage(back_render_stage);

    // Save the output assuming one output
    GID output_node = m_outputs[0];

    m_render_order.clear();
    m_outputs.clear();
    m_inputs.clear();

    // Re-generate graph
    if (!construct_render_order(find_render_stage(output_node))) {
        // Undo the connection
        throw std::runtime_error("Failed to construct render order.");
    }

    printf("Render stage order: ");
    for (auto & gid : m_render_order) {
        printf("%d ", gid);
    }
    printf("\n");

    return true;
}