#ifndef CONTENT_SCALING_H
#define CONTENT_SCALING_H

#include <array>

namespace ContentScaling {

    // TODO: Add functionatity for padding and margins in the future

enum class ScaleMode {
    STRETCH,     // Stretch content to fill frame (ignore aspect ratio)
    FIT,         // Scale to fit entirely within frame (maintain aspect ratio, may have letterboxing)
    FILL,        // Scale to fill entire frame (maintain aspect ratio, may crop content)
    ORIGINAL     // Use original content size (no scaling)
};

struct ScalingParams {
    ScaleMode scale_mode = ScaleMode::FIT;
    float horizontal_alignment = 0.5f; // 0.0 = left, 0.5 = center, 1.0 = right
    float vertical_alignment = 0.5f;   // 0.0 = top, 0.5 = center, 1.0 = bottom
    float custom_aspect_ratio = 1.0f;  // 0 means use content's natural aspect ratio
};

// Calculate vertex data for rendering content with proper scaling and alignment
// Returns array of 24 floats representing 6 vertices (2 position + 2 texcoord each)
std::array<float, 24> calculateVertexData(
    float content_aspect_ratio,         // Width/height ratio of the content
    float component_aspect_ratio,       // Width/height ratio of the component/frame
    float display_aspect_ratio,         // Width/height ratio of the display
    const ScalingParams& params         // Scaling and alignment parameters
);

} // namespace ContentScaling

#endif // CONTENT_SCALING_H