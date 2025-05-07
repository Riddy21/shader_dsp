#include "graphics_views/debug_view.h"

#include "audio_synthesizer/audio_synthesizer.h"
#include "graphics_components/graph_component.h"

DebugView::DebugView() {

    auto & audio_synthesizer = AudioSynthesizer::get_instance();

    const auto & channel_seperated_audio = audio_synthesizer.get_audio_data();

    // Specify position, width, and height for the graph
    for (size_t i = 0; i < channel_seperated_audio.size(); ++i) {
        float width = 2.0f;
        float height = 2.0f / channel_seperated_audio.size();
        float x = 0.0f;
        float y = i * height;
        auto graph = new GraphComponent(x, y, width, height, channel_seperated_audio[i], true);
        add_component(graph);
    }
}

void DebugView::on_render() {
    // Call the base class render method
    GraphicsView::on_render();
}