#include "catch2/catch_all.hpp"
#include <memory>
#include <variant>
#include "audio_core/audio_control.h"
#include "audio_synthesizer/audio_module.h"
#include "audio_render_stage/audio_effect_render_stage.h"
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
