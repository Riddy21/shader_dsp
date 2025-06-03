#include "graphics_core/content_scaling.h"

namespace ContentScaling {

std::array<float, 24> calculateVertexData(
    float content_aspect_ratio,  
    float component_aspect_ratio,
    float display_aspect_ratio,
    const ScalingParams& params
) {
    std::array<float, 24> vertex_data;

    // Calculate scaling factors
    float width_scale = 1.0f;
    float height_scale = 1.0f;

    // Adjust for custom aspect ratio if specified
    float effective_aspect_ratio = content_aspect_ratio;
    effective_aspect_ratio /= params.custom_aspect_ratio;

    // Adjust aspect ratios to account for display aspect ratio
    effective_aspect_ratio /= display_aspect_ratio;
    float frame_aspect = component_aspect_ratio;

    // Calculate scaling based on scale mode
    switch (params.scale_mode) {
        case ScaleMode::STRETCH:
            width_scale = 1.0f;
            height_scale = 1.0f;
            break;

        case ScaleMode::FIT:
            if (effective_aspect_ratio > component_aspect_ratio) {
                width_scale = 1.0f;
                height_scale = component_aspect_ratio / effective_aspect_ratio;
            } else {
                width_scale = effective_aspect_ratio / component_aspect_ratio;
                height_scale = 1.0f;
            }
            break;

        case ScaleMode::FILL:
            if (effective_aspect_ratio > component_aspect_ratio) {
                width_scale = effective_aspect_ratio / component_aspect_ratio;
                height_scale = 1.0f;
            } else {
                width_scale = 1.0f;
                height_scale = component_aspect_ratio / effective_aspect_ratio;
            }
            break;
    }

    // Apply alignment (use float values directly)
    float h_align = params.horizontal_alignment;
    float v_align = params.vertical_alignment;

    // Calculate offsets based on alignment
    float x_offset = -1.0f + 2.0f * h_align * (1.0f - width_scale);
    float y_offset = 1.0f - 2.0f * v_align * (1.0f - height_scale);

    // Bottom left triangle
    vertex_data[0]  = x_offset;
    vertex_data[1]  = y_offset - 2.0f * height_scale;
    vertex_data[2]  = 0.0f;
    vertex_data[3]  = 1.0f;

    vertex_data[4]  = x_offset;
    vertex_data[5]  = y_offset;
    vertex_data[6]  = 0.0f;
    vertex_data[7]  = 0.0f;

    vertex_data[8]  = x_offset + 2.0f * width_scale;
    vertex_data[9]  = y_offset;
    vertex_data[10] = 1.0f;
    vertex_data[11] = 0.0f;

    // Top right triangle
    vertex_data[12] = x_offset;
    vertex_data[13] = y_offset - 2.0f * height_scale;
    vertex_data[14] = 0.0f;
    vertex_data[15] = 1.0f;

    vertex_data[16] = x_offset + 2.0f * width_scale;
    vertex_data[17] = y_offset;
    vertex_data[18] = 1.0f;
    vertex_data[19] = 0.0f;

    vertex_data[20] = x_offset + 2.0f * width_scale;
    vertex_data[21] = y_offset - 2.0f * height_scale;
    vertex_data[22] = 1.0f;
    vertex_data[23] = 1.0f;
    return vertex_data;
}

} // namespace ContentScaling