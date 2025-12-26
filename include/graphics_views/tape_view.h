#pragma once
#ifndef TAPE_VIEW_H
#define TAPE_VIEW_H

#include "graphics_core/graphics_view.h"
#include "graphics_components/image_component.h"

class TapeView : public GraphicsView {
public:
    TapeView();
    ~TapeView() override = default;
    
    void render() override;

private:
    ImageComponent* m_tape_wheel_1 = nullptr;
    ImageComponent* m_tape_wheel_2 = nullptr;
    float m_rotation_speed = 1.0f; // Rotation speed in radians per second
};

#endif // TAPE_VIEW_H

