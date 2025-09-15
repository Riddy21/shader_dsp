#include "catch2/catch_all.hpp"
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>
#include <vector>

#include "graphics_core/graphics_display.h"
#include "graphics_core/graphics_view.h"
#include "engine/event_handler.h"
#include "graphics_components/button_component.h"
#include "graphics_components/graph_component.h"
#include "graphics_components/image_button_component.h"
#include "graphics_components/image_component.h"
#include "graphics_components/menu_item_component.h"
#include "graphics_components/menu_selection_component.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/text_component.h"

struct SDLInitGuard {
    SDLInitGuard() {
        if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
            SDL_Init(SDL_INIT_EVERYTHING);
            m_we_initialised = true;
        }
    }
    ~SDLInitGuard() {
        if (m_we_initialised) {
            SDL_Quit();
        }
    }
private:
    bool m_we_initialised = false;
};


class TestView1 : public GraphicsView {
public:
    TestView1() {
        auto text = new TextComponent(-0.975f, 0.96667f, 0.5f, 0.16667f, "Hello World");
        add_component(text);

        auto image = new ImageComponent(-0.975f, 0.76667f, 0.5f, 0.66667f, "media/icons/dice.png");
        add_component(image);

        auto button = new ButtonComponent(-0.975f, -0.03333f, 0.5f, 0.16667f, [](){ /* dummy */ });
        add_component(button);
    }
};

class TestView2 : public GraphicsView {
public:
    TestView2() {
        auto text_button = new TextButtonComponent(-0.975f, 0.96667f, 0.5f, 0.16667f, "Click Me", [](){ /* dummy */ });
        add_component(text_button);

        auto image_button = new ImageButtonComponent(-0.975f, 0.76667f, 0.5f, 0.66667f, "media/icons/dice.png", [](){ /* dummy */ });
        add_component(image_button);

        static std::vector<float> graph_data = {0.1f, 0.2f, 0.4f, 0.8f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        auto graph = new GraphComponent(-0.975f, -0.03333f, 0.5f, 0.66667f, graph_data);
        add_component(graph);
    }
};

class TestView3 : public GraphicsView {
public:
    TestView3() {
        auto menu_item = new MenuItemComponent(-0.975f, 0.96667f, 0.5f, 0.16667f, "Menu Item", 0);
        add_component(menu_item);

        std::vector<std::string> items = {"Option 1", "Option 2", "Option 3"};
        auto menu_selection = new MenuSelectionComponent(-0.975f, 0.76667f, 0.5f, 0.66667f, items);
        add_component(menu_selection);
    }
};

TEST_CASE("All components test scene", "[graphics][.]") {
    SDLInitGuard sdl_guard;

    EventLoop& event_loop = EventLoop::get_instance();

    EventHandler& eh = EventHandler::get_instance();

    // Create display first
    GraphicsDisplay display(800, 600, "Test Scene");

    // Create views and add components
    GraphicsView* view1 = new TestView1();
    display.add_view("view1", view1);  // Initializes with proper context

    GraphicsView* view2 = new TestView2();
    display.add_view("view2", view2);

    GraphicsView* view3 = new TestView3();
    display.add_view("view3", view3);

    display.change_view("view1");

    // Set up switching
    std::vector<std::string> view_names = {"view1", "view2", "view3"};
    size_t current_index = 0;

    eh.register_entry(new KeyboardEventHandlerEntry(SDL_KEYDOWN, SDLK_SPACE, [&display, &view_names, &current_index](const SDL_Event& e) {
        current_index = (current_index + 1) % view_names.size();
        display.change_view(view_names[current_index]);
        return true;
    }));

    event_loop.run_loop();
}
