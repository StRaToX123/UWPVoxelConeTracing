#pragma once


#include <DirectXMath.h>
#include <d3d12.h>
#include <vector>
#include <stdexcept>
#include <wrl.h>

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
        static void ReverseWinding(vector<uint16_t>& indices, vector<VertexPositionNormalTexture>& vertices);

        vector<VertexPositionNormalTexture> vertices;
        vector<uint16_t> indices;

        XMFLOAT3 world_position;
        XMFLOAT3 local_rotation;

        Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer_upload;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
           
        Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer_upload;
        D3D12_INDEX_BUFFER_VIEW index_buffer_view;

        UINT64 fence_value_signaling_buffers_are_present_on_the_gpu;
        bool buffer_upload_started;

    private:
        
        // Computes a point on a unit circle, aligned to the x/z plane and centered on the origin.
        inline XMVECTOR GetCircleVector(size_t i, size_t tessellation);
        XMVECTOR GetCircleTangent(size_t i, size_t tessellation);
        // Helper creates a triangle fan to close the end of a cylinder / cone
        void CreateCylinderCap(vector<VertexPositionNormalTexture>& vertices, vector<uint16_t>& indices, size_t tessellation, float height, float radius, bool isTop);

        
};