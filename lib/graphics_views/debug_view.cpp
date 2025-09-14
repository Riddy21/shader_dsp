#include "graphics_views/debug_view.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "graphics_components/graph_component.h"
#include "graphics_components/menu_item_component.h"

DebugView::DebugView(const std::vector<const std::vector<float>*>& data)
    : GraphicsView()
{
    // Use the passed data pointers instead of getting it from audio synthesizer
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != nullptr) { // Check for null pointer
            float width = 2.0f;
            float height = 2.0f / data.size();
            float x = -1.0f;
            float y = -(i * height) + 1.0;
            auto graph = new GraphComponent(x, y, width, height, *data[i], true);
            add_component(graph);
        }
    }
}

DebugView::DebugView()
    : GraphicsView()
{
    auto & audio_synthesizer = AudioSynthesizer::get_instance();
    const auto & channel_seperated_audio = audio_synthesizer.get_audio_data();

    // Specify position, width, and height for the graph
    for (size_t i = 0; i < channel_seperated_audio.size(); ++i) {
        float width = 2.0f;
        float height = 2.0f / channel_seperated_audio.size();
        float x = -1.0f;
        float y = -(i * height) + 1.0;
        auto graph = new GraphComponent(x, y, width, height, channel_seperated_audio[i], true);
        add_component(graph);
    }
}