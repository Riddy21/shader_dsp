#include "catch2/catch_all.hpp"
#include <memory>
#include <variant>
#include "audio_core/audio_control.h"
#include "audio_synthesizer/audio_module.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_renderer.h"

TEST_CASE("AudioEffectModule with multiple render stages exposes controls", "[audio_module]") {
    // Get audio renderer singleton instance
    auto& renderer = AudioRenderer::get_instance();
    
    unsigned int frames_per_buffer = 128;
    unsigned int sample_rate = 44100;
    unsigned int num_channels = 2;
    //renderer.initialize(frames_per_buffer, sample_rate, num_channels);

    // Construct two effect render stages
    auto stage1 = std::make_shared<AudioGainEffectRenderStage>(
        frames_per_buffer, sample_rate, num_channels
    );
    auto stage2 = std::make_shared<AudioEchoEffectRenderStage>(
        frames_per_buffer, sample_rate, num_channels
    );
    
    // Create vector with initialized stages
    std::vector<std::shared_ptr<AudioRenderStage>> stages = {stage1, stage2};

    // Construct the effect module with initialized stages
    AudioEffectModule module("test_effect", stages);
}

TEST_CASE("AudioModuleManager can add and replace modules", "[audio_module_manager]") {
    // Get audio renderer singleton instance
    auto& renderer = AudioRenderer::get_instance();
    
    unsigned int frames_per_buffer = 128;
    unsigned int sample_rate = 44100;
    unsigned int num_channels = 2;

    // Create a render graph with a dummy root stage
    auto * root_stage = new AudioFinalRenderStage(frames_per_buffer, sample_rate, num_channels);
    AudioRenderGraph render_graph(root_stage);

    // add render graph to the audio renderer
    if (!renderer.add_render_graph(&render_graph)) {
        FAIL("Failed to add render graph to audio renderer.");
    }

    renderer.initialize(frames_per_buffer, sample_rate, num_channels);
    
    AudioModuleManager module_manager(render_graph, root_stage->gid, renderer);

    // Create an effect module
    auto stage1 = std::make_shared<AudioGainEffectRenderStage>(
        frames_per_buffer, sample_rate, num_channels
    );
    auto stage2 = std::make_shared<AudioEchoEffectRenderStage>(
        frames_per_buffer, sample_rate, num_channels
    );

    REQUIRE(stage1->initialize());
    REQUIRE(stage2->initialize());
    
    std::vector<std::shared_ptr<AudioRenderStage>> stages = {stage1, stage2};
    
    auto effect_module = std::make_shared<AudioEffectModule>("test_effect_1", stages);
    
    // Add the module to the manager
    module_manager.add_module(effect_module);
    
    // Check if the module is added
    REQUIRE(module_manager.get_module_names().size() == 1);
    REQUIRE(module_manager.get_module_names()[0] == "test_effect_1");

    auto & render_order = render_graph.get_render_order();
    REQUIRE(render_order.size() == 3);
    REQUIRE(render_order[0] == stage1->gid);
    REQUIRE(render_order[1] == stage2->gid);
    REQUIRE(render_order[2] == root_stage->gid);
    
    // Replace the module with a new one
    auto new_stage1 = std::make_shared<AudioGainEffectRenderStage>(
        frames_per_buffer, sample_rate, num_channels
    );
    
    auto new_effect_module = std::make_shared<AudioEffectModule>("test_effect_2", std::vector<std::shared_ptr<AudioRenderStage>>{new_stage1});
    
    auto old_module = module_manager.replace_module("test_effect_1", new_effect_module);
    
    // Check if the old module is returned and replaced correctly
    REQUIRE(old_module->name() == "test_effect_1");
    REQUIRE(module_manager.get_module_names().size() == 1);
    REQUIRE(module_manager.get_module_names()[0] == "test_effect_2");

    // Check the render order size and the value
    auto & render_order_1 = render_graph.get_render_order();
    REQUIRE(render_order_1.size() == 2);
    REQUIRE(render_order_1[0] == new_stage1->gid);
    REQUIRE(render_order_1[1] == root_stage->gid);
}