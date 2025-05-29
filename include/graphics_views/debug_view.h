#pragma once
#include "graphics_core/graphics_view.h"
#include "engine/event_handler.h"

class DebugView : public GraphicsView {
public:
    // Constructor with event handler and render context
    DebugView(EventHandler& event_handler, const RenderContext& render_context);
    
    ~DebugView() override = default;
};
