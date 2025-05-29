#include <iostream>

#include "graphics_views/menu_view.h"
#include "graphics_components/menu_selection_component.h"

MenuView::MenuView(EventHandler& event_handler, const RenderContext& render_context)
    : GraphicsView(event_handler, render_context)
{
    // Create menu component
    auto menu = new MenuSelectionComponent(
        -0.5f, 0.0f, 1.0f, 2.0f,
        {"Start Game", "Options", "Exit"},
        [](int index) {
            // Handle selection callback
            switch (index) {
                case 0:
                    std::cout << "Starting game..." << std::endl;
                    break;
                case 1:
                    std::cout << "Opening options..." << std::endl;
                    break;
                case 2:
                    std::cout << "Exiting..." << std::endl;
                    break;
            }
        },
        m_event_handler,
        m_render_context
    );

    add_component(menu);
}
