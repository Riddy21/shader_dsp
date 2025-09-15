#pragma once
#include "graphics_core/graphics_view.h"
#include <vector>

class DebugView : public GraphicsView {
public:
    DebugView(const std::vector<const std::vector<float>*>& data);
    DebugView(); // Default constructor for backward compatibility
    ~DebugView() override = default;
};
