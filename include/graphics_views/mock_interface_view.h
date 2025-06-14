#ifndef MOCK_INTERFACE_VIEW_H
#define MOCK_INTERFACE_VIEW_H

#include "graphics_core/graphics_view.h"
#include "graphics_components/button_component.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/image_button_component.h"
#include "graphics_components/text_component.h"
#include "graphics_components/image_component.h"
#include "audio_synthesizer/audio_synthesizer.h"

class IRenderableEntity;
class GraphicsDisplay;

class MockInterfaceView : public GraphicsView {
public:
    MockInterfaceView();
    ~MockInterfaceView() override = default;
    
    // Debug helpers
    void toggle_component_outlines();

protected:
    
private:
};

#endif // MOCK_INTERFACE_VIEW_H