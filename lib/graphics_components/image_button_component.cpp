#include "graphics_components/image_button_component.h"

ImageButtonComponent::ImageButtonComponent(
    float x, float y, float width, float height, 
    const std::string& image_path, ButtonCallback callback
) : ButtonComponent(x, y, width, height, callback),
    m_image_component(nullptr)
{
    // Create the image component as a child of this button
    m_image_component = new ImageComponent(x, y, width, height, image_path);
    
    // Add the raw pointer to the GraphicsComponent's children list
    // This transfers ownership to the parent component
    add_child(m_image_component);
}

ImageButtonComponent::~ImageButtonComponent()
{
    // No need to manually delete m_image_component as it's managed by the parent
}

bool ImageButtonComponent::load_image(const std::string& image_path) {
    return m_image_component ? m_image_component->load_image(image_path) : false;
}

bool ImageButtonComponent::load_from_surface(SDL_Surface* surface) {
    return m_image_component ? m_image_component->load_from_surface(surface) : false;
}

void ImageButtonComponent::set_tint_color(float r, float g, float b, float a) {
    m_tint_color[0] = r;
    m_tint_color[1] = g;
    m_tint_color[2] = b;
    m_tint_color[3] = a;
    
    if (m_image_component) {
        m_image_component->set_tint_color(r, g, b, a);
    }
}

void ImageButtonComponent::set_hover_tint_color(float r, float g, float b, float a) {
    m_hover_tint_color[0] = r;
    m_hover_tint_color[1] = g;
    m_hover_tint_color[2] = b;
    m_hover_tint_color[3] = a;
    
    // Apply immediately if the button is currently hovered
    if (is_hovered() && m_image_component) {
        m_image_component->set_tint_color(r, g, b, a);
    }
}

void ImageButtonComponent::set_active_tint_color(float r, float g, float b, float a) {
    m_active_tint_color[0] = r;
    m_active_tint_color[1] = g;
    m_active_tint_color[2] = b;
    m_active_tint_color[3] = a;
    
    // Apply immediately if the button is currently pressed
    if (is_pressed() && m_image_component) {
        m_image_component->set_tint_color(r, g, b, a);
    }
}

void ImageButtonComponent::set_scale_mode(ImageComponent::ScaleMode mode) {
    if (m_image_component) {
        m_image_component->set_scale_mode(mode);
    }
}

void ImageButtonComponent::update_children() {
    // Update image tint color based on button state
    if (!m_image_component) return;
    
    if (is_pressed()) {
        m_image_component->set_tint_color(
            m_active_tint_color[0], 
            m_active_tint_color[1], 
            m_active_tint_color[2], 
            m_active_tint_color[3]
        );
    } else if (is_hovered()) {
        m_image_component->set_tint_color(
            m_hover_tint_color[0], 
            m_hover_tint_color[1], 
            m_hover_tint_color[2], 
            m_hover_tint_color[3]
        );
    } else {
        m_image_component->set_tint_color(
            m_tint_color[0], 
            m_tint_color[1], 
            m_tint_color[2], 
            m_tint_color[3]
        );
    }
}