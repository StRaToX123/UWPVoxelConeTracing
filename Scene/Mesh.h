#pragma once


#include <DirectXMath.h>
#include <d3d12.h>
#include <vector>
#include <stdexcept>

using namespace DirectX;
using namespace std;

// Vertex struct holding position, normal vector, and texture mapping information.
struct VertexPositionNormalTexture
{
    VertexPositionNormalTexture()
    { 
    
    }

    VertexPositionNormalTexture(const XMFLOAT3& position, const XMFLOAT3& normal, const XMFLOAT2& textureCoordinate)
        : position(position),
          normal(normal),
          textureCoordinate(textureCoordinate)
    { 
    
    }

    VertexPositionNormalTexture(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
    {
        XMStoreFloat3(&this->position, position);
        XMStoreFloat3(&this->normal, normal);
        XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
    }

    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 textureCoordinate;

    static const int input_element_count = 3;
    static const D3D12_INPUT_ELEMENT_DESC input_elements[input_element_count];
};



class Mesh
{
    public:
        Mesh();
    
        void InitializeAsCube(float size = 1);
        void InitializeAsSphere(float diameter = 1, size_t tessellation = 16);
        void InitializeAsCone(float diameter = 1, float height = 1, size_t tessellation = 32);
        void InitializeAsTorus(float diameter = 1, float thickness = 0.333f, size_t tessellation = 32);
        void InitializeAsPlane(float width = 1, float height = 1);

    private:
        void ReverseWinding(vector<uint16_t>& indices, vector<VertexPositionNormalTexture>& vertices);
        // Computes a point on a unit circle, aligned to the x/z plane and centered on the origin.
        inline XMVECTOR GetCircleVector(size_t i, size_t tessellation);
        XMVECTOR GetCircleTangent(size_t i, size_t tessellation);

        vector<VertexPositionNormalTexture> vertices;
        vector<uint16_t> indices;
};