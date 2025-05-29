#pragma once
#include "graphics_core/graphics_view.h"
#include "engine/event_handler.h"

class MenuView : public GraphicsView {
public:
    MenuView(EventHandler& event_handler, const RenderContext& render_context);
    ~MenuView() override = default;
};
