#pragma once
#ifndef AUDIO_RENDER_GRAPH_H
#define AUDIO_RENDER_GRAPH_H

#include <unordered_set>
#include <string>
#include <unordered_map>
#include <memory>

#include "audio_render_stage/audio_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"

//       This graph should be able to:
//       - save the render stages in a graph format with the root as the output node
//       - be able to traverse the graph in a topological order
//       - be able to initialize the render stages in the correct order
//       - be able to link the render stages together
//       - be able to find the render stage by name
//       - be able to find the render stage by global ID
//       - be able to dynamically add render stages and replace render stages

// Initilaization steps
//       - pass in the input render stages
//       - traverse the graph
//             - construct graph representation
//             - construct render order representation
//             - construct dictionary representation
//       - check the output exists
//       - check no loops in the graph
//       - check the graph is connected

// Other functions
//       - insert node between 2 nodes
//       - remove node
//       - replace node

class AudioRenderGraph {
public:
    using GID = unsigned int;

    AudioRenderGraph(const std::vector<AudioRenderStage *> inputs);

    ~AudioRenderGraph();

    bool initialize();

    bool bind_render_stages();

    void render();

    AudioRenderStage * find_render_stage(GID gid);

    // TODO: To implement
    bool replace_render_stage(GID gid, AudioRenderStage * render_stage);

    // TODO: To implement
    bool remove_render_stage(GID gid);

    // TODO: To implement
    bool insert_render_stage_infront(GID back, AudioRenderStage * render_stage);
    
    // TODO: To implement
    bool insert_render_stage_behind(GID front, AudioRenderStage * render_stage);

    // TODO: To implement
    bool insert_render_stage_between(GID front, GID back, AudioRenderStage * render_stage);

    // Getters
    AudioFinalRenderStage * get_output_render_stage() {
        return static_cast<AudioFinalRenderStage *>(find_render_stage(m_outputs[0]));
    }

private:
    bool construct_render_order(GID node, std::unordered_set<GID> & visited);

    bool construct_graph(const std::vector<AudioRenderStage *> & inputs);
    bool dfs_construct_graph(AudioRenderStage * node, std::unordered_set<GID> & visited);

    static std::vector<AudioRenderStage *> find_linked_render_stages(const AudioRenderStage * render_stage);

    std::unordered_map<GID, std::unordered_set<GID>> m_graph;
    std::vector<GID> m_outputs;
    std::vector<GID> m_inputs;
    std::vector<GID> m_render_order;

    std::unordered_map<GID, std::unique_ptr<AudioRenderStage>> m_render_stages_map;
};

#endif