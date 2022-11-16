#pragma once

#include <DirectXMath.h>


struct ShaderStructureCPUSpotLight
{
    ShaderStructureCPUSpotLight()
        : PositionWS(0.0f, 0.0f, 0.0f, 1.0f)
        , PositionVS(0.0f, 0.0f, 0.0f, 1.0f)
        , DirectionWS(0.0f, 0.0f, 1.0f, 0.0f)
        , DirectionVS(0.0f, 0.0f, 1.0f, 0.0f)
        , Color(1.0f, 1.0f, 1.0f, 1.0f)
        , Intensity(1.0f)
        , SpotAngle(DirectX::XM_PIDIV2)
        , Attenuation(0.0f)
    {}

    DirectX::XMFLOAT4    PositionWS; // Light position in world space.
    //----------------------------------- (16 byte boundary)
    DirectX::XMFLOAT4    PositionVS; // Light position in view space.
    //----------------------------------- (16 byte boundary)
    DirectX::XMFLOAT4    DirectionWS; // Light direction in world space.
    //----------------------------------- (16 byte boundary)
    DirectX::XMFLOAT4    DirectionVS; // Light direction in view space.
    //----------------------------------- (16 byte boundary)
    DirectX::XMFLOAT4    Color;
    //----------------------------------- (16 byte boundary)
    float       Intensity;
    float       SpotAngle;
    float       Attenuation;
    float       Padding;                // Pad to 16 bytes.
    //----------------------------------- (16 byte boundary)
    // Total:                              16 * 6 = 96 bytes
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
