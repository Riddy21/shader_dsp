#pragma once
#ifndef AUDIO_RENDER_GRAPH_H
#define AUDIO_RENDER_GRAPH_H

#include <unordered_set>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

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

    AudioRenderGraph(AudioRenderStage * output);
    // TODO: Consider making the render graph input driven graph so it checks the render stages
    AudioRenderGraph(std::vector<AudioRenderStage *> inputs);

    ~AudioRenderGraph();

    bool initialize();

    void render();

    void update();

    AudioRenderStage * find_render_stage(GID gid) {return m_render_stages_map[gid].get();}

    // TODO: To implement
    std::unique_ptr<AudioRenderStage> replace_render_stage(GID gid, AudioRenderStage * render_stage);

    std::unique_ptr<AudioRenderStage> remove_render_stage(GID gid);

    bool insert_render_stage_infront(GID back, AudioRenderStage * render_stage);
    
    bool insert_render_stage_behind(GID front, AudioRenderStage * render_stage);

    bool insert_render_stage_between(GID front, GID back, AudioRenderStage * render_stage);

    // Getters
    AudioFinalRenderStage * get_output_render_stage() {
        return dynamic_cast<AudioFinalRenderStage *>(find_render_stage(m_outputs[0]));
    }

private:
    static AudioRenderStage * from_input_to_output(AudioRenderStage * node, std::unordered_set<GID> & visited);
    bool construct_render_order(AudioRenderStage * node);
    bool bind_render_stages();

    std::vector<GID> m_outputs;
    std::vector<GID> m_inputs;
    std::vector<GID> m_render_order;

    std::mutex m_graph_mutex;

    bool m_needs_update = false;

    std::unordered_map<GID, std::unique_ptr<AudioRenderStage>> m_render_stages_map;
};

#endif