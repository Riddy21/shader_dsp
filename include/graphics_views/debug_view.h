#pragma once
#include "graphics_core/graphics_view.h"
#include "graphics/graphics_display.h"


class DebugView : public GraphicsView {
public:
    DebugView(GraphicsDisplay* parent_display, EventHandler* event_handler);
    ~DebugView() override = default;

    void render() override;
};
