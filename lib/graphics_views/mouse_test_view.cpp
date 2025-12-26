#include "graphics_views/mouse_test_view.h"
#include "graphics_components/mouse_test_component.h"

MouseTestView::MouseTestView() : GraphicsView() {
    // Create a full-screen component (-1, 1) to (1, -1) covers the entire screen
    // In normalized coordinates: x from -1 to 1, y from 1 to -1, width=2, height=2
    auto mouse_test_component = new MouseTestComponent(-1.0f, 1.0f, 2.0f, 2.0f);
    add_component(mouse_test_component);
}

