#ifndef IMAGE_BUTTON_COMPONENT_H
#define IMAGE_BUTTON_COMPONENT_H

#include <memory>
#include <string>
#include "graphics_components/button_component.h"
#include "graphics_components/image_component.h"

class ImageButtonComponent : public ButtonComponent {
public:
    ImageButtonComponent(
        PositionMode position_mode, float x, float y, float width, float height, 
        const std::string& image_path, ButtonCallback callback
    );
    ImageButtonComponent(
        float x, float y, float width, float height, 
        const std::string& image_path, ButtonCallback callback
    );
    ~ImageButtonComponent() override;

    // Image-specific methods
    bool load_image(const std::string& image_path);
    bool load_from_surface(SDL_Surface* surface);
    
    void set_tint_color(float r, float g, float b, float a);
    void set_hover_tint_color(float r, float g, float b, float a);
    void set_active_tint_color(float r, float g, float b, float a);
    
    void set_scale_mode(ImageComponent::ScaleMode mode);
    
    // Override to update image component based on button state
    void update_children() override;

private:
    ImageComponent* m_image_component;
    
    // Store tint colors for different states
    float m_tint_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_hover_tint_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_active_tint_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

#endif // IMAGE_BUTTON_COMPONENT_H