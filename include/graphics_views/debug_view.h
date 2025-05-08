#pragma once
#include "graphics_core/graphics_view.h"


class DebugView : public GraphicsView {
public:
    DebugView();
    ~DebugView() override = default;

    void render() override;
};
