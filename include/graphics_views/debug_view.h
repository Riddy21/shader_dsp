#pragma once
#include "graphics_core/graphics_view.h"
#include "graphics/graphics_display.h"


class DebugView : public GraphicsView {
public:
    DebugView();
    ~DebugView() override = default;
};
