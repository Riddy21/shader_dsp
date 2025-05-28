#include <iostream>

#include "graphics_views/menu_view.h"

#include "graphics_components/menu_selection_component.h"

MenuView::MenuView()
    : GraphicsView()
{
    // Empty for now
    auto menu = new MenuSelectionComponent(
        0.0f, 0.0f, 1.0f, 1.0f,
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
        }
    );

    add_component(menu);
}
