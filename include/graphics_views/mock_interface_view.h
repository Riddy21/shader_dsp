#ifndef MOCK_INTERFACE_VIEW_H
#define MOCK_INTERFACE_VIEW_H

#include "graphics_core/graphics_view.h"
#include "graphics_components/button_component.h"
#include "audio_synthesizer/audio_synthesizer.h"

class IRenderableEntity;
class GraphicsDisplay;

class MockInterfaceView : public GraphicsView {
public:
    MockInterfaceView(GraphicsDisplay* parent_display, EventHandler* event_handler);
    ~MockInterfaceView() override = default;

protected:
    // Override to register view-specific event handlers
    void register_event_handler(EventHandler* event_handler) override ;
    void unregister_event_handler(EventHandler* event_handler) override ;
    
private:
};

#endif // MOCK_INTERFACE_VIEW_H