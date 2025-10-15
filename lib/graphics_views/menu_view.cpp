#include <iostream>

#include "graphics_views/menu_view.h"

#include "audio_synthesizer/audio_synthesizer.h"
#include "graphics_components/menu_selection_component.h"

MenuView::MenuView()
    : GraphicsView()
{
    auto & registry = AudioControlRegistry::instance();
    auto & effect_control = registry.get_control({"current", "menu_effect"});
    auto & voice_control = registry.get_control({"current", "menu_voice"});

    auto menu = new MenuSelectionComponent(
        -1.0f, 1.0f, 1.0f, 2.0f,
        effect_control->items<std::string>(),
        [&effect_control](std::string title) {
            effect_control->set<std::string>(title);
        }
    );
    add_component(menu);

    auto menu2 = new MenuSelectionComponent(
        0.0f, 1.0f, 1.0f, 2.0f,
        voice_control->items<std::string>(),
        [&voice_control](std::string title) {
            voice_control->set<std::string>(title);
        }
    );
    add_component(menu2);
}