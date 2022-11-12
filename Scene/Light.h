#pragma once

#include <DirectXMath.h>


struct ShaderStructureSpotLight
{
    ShaderStructureSpotLight() :
        world_position(0.0f, 0.0f, 0.0f, 1.0f),
        direction_world_space(0.0f, 0.0f, 1.0f, 0.0f),
        color(1.0f, 1.0f, 1.0f, 1.0f),
        intensity(1.0f),
        spot_angle(90.0f),
        attenuation(0.0f)
    {

    }

    DirectX::XMFLOAT4    world_position;
    DirectX::XMFLOAT4    direction_world_space;
    DirectX::XMFLOAT4    color;

    float       intensity;
    float       spot_angle;
    float       attenuation;
    float       padding;
};

struct ShaderStructureDirectionalLight
{
    ShaderStructureDirectionalLight() :
          direction_world_space(0.0f, 0.0f, 1.0f, 0.0f),
          color(1.0f, 1.0f, 1.0f, 1.0f)
    {

    }

    DirectX::XMFLOAT4    direction_world_space;
    DirectX::XMFLOAT4    color;
};
