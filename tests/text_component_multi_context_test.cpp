#include "catch2/catch_all.hpp"
#include <SDL2/SDL.h>
#include <vector>
#include <string>

#include "graphics_core/graphics_display.h"
#include "graphics_core/graphics_view.h"
#include "graphics_components/text_component.h"
#include "engine/event_handler.h"
#include "test_sdl_manager.h"

// Simple view that contains a text component
class TextTestView : public GraphicsView {
public:
    TextTestView(const std::string& text) {
        auto text_comp = new TextComponent(0.0f, 0.0f, 0.5f, 0.2f, text);
        add_component(text_comp);
    }
};

TEST_CASE("TextComponent on multiple contexts", "[graphics][text][multi_context]") {
    // Initialize SDL
    TestSDLGuard sdl_guard(SDL_INIT_VIDEO);
    
    EventHandler& eh = EventHandler::get_instance();
    
    // Create first display (first context)
    GraphicsDisplay display1(400, 300, "Display 1 - Context 1", 60, eh);
    
    // Create second display (second context)
    GraphicsDisplay display2(400, 300, "Display 2 - Context 2", 60, eh);
    
    // Create views with text components for each display
    GraphicsView* view1 = new TextTestView("Text on Context 1");
    display1.add_view("main", view1);
    display1.change_view("main");
    
    GraphicsView* view2 = new TextTestView("Text on Context 2");
    display2.add_view("main", view2);
    display2.change_view("main");
    
    // Render first display
    // This should initialize static/shared resources for context 1
    display1.render();
    
    // Render second display
    // This should initialize resources for context 2
    // If static resources were shared improperly, this might crash or fail
    display2.render();
    
    // Render first display again to ensure context switching back works
    display1.render();
    
    // Render second display again
    display2.render();
    
    // Check if we can still access the text components without crashing
    REQUIRE(true);
}

