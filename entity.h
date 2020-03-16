#pragma once

#include "shared.h"
#include "renderer.h"

struct entity
{
    texture *Texture;

    glm::vec3 Acceleration;
    glm::vec3 Velocity;
    glm::vec3 Position;
    glm::vec3 Size;
    glm::vec3 Direction;

    f32 Speed;
    f32 RotationAngle; // NOTE: Degrees!
    f32 DragCoefficient;
};
