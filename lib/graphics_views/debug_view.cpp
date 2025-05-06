#include "graphics_views/debug_view.h"

#include "graphics_components/graph_component.h"

DebugView::DebugView() {
    // Make a temporary sine wave graph
    const size_t num_samples = 100;
    std::vector<float> wave_data(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        wave_data[i] = -1.0f + 2.0f * static_cast<float>(i) / (num_samples - 1); // Linearly interpolate from -1 to 1
    }

    // Specify position, width, and height for the graph
    float x = 0.0f, y = 0.0f, width = 2.0f, height = 2.0f;
    auto graph = new GraphComponent(x, y, width, height, wave_data.data(), num_samples);
    add_component(graph);
}

void DebugView::on_render() {
    // Call the base class render method
    GraphicsView::on_render();

}