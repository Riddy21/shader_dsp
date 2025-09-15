#include <iostream>

#include "graphics_views/menu_view.h"

#include "audio_synthesizer/audio_synthesizer.h"
#include "graphics_components/menu_selection_component.h"

MenuView::MenuView()
    : GraphicsView()
{
    auto & synthesizer = AudioSynthesizer::get_instance();

    auto & track = synthesizer.get_track(0);

    auto menu = new MenuSelectionComponent(
        -1.0f, 1.0f, 1.0f, 2.0f,
        track.get_effect_names(),
        [&track, this](std::string title) {
            track.change_effect(title);
        }
    );
    add_component(menu);

    auto menu2 = new MenuSelectionComponent(
        0.0f, 1.0f, 1.0f, 2.0f,
        track.get_generator_names(),
        [&track](std::string title) {
            track.change_voice(title);
        }
    );
    add_component(menu2);
}