#include <iostream>

#include "graphics_views/menu_view.h"

#include "audio_synthesizer/audio_synthesizer.h"
#include "graphics_components/menu_selection_component.h"

MenuView::MenuView()
    : GraphicsView()
{
    auto & synthesizer = AudioSynthesizer::get_instance();

    auto & track = synthesizer.get_track(0);

    // Empty for now
    // FIXME: Menu is misaligned
    auto menu = new MenuSelectionComponent(
        -0.5f, 0.0f, 1.0f, 2.0f,
        track.get_effect_names(),
        // TODO: Track the title of the component instead of the index
        [&track](std::string title) {
            track.change_effect(title);
        }
    );
    add_component(menu);
}
