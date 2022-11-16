#pragma once


#include <DirectXMath.h>
#include <d3d12.h>
#include <vector>
#include <stdexcept>
#include <wrl.h>
#include "Utility/Debugging/DebugMessage.h"
#include "Graphics/DeviceResources/DeviceResourcesCommon.h"

using namespace DirectX;
using namespace std;

// Used to send per-vertex data to the vertex shader.
// Vertex struct holding position, normal vector, and texture mapping information.
struct ShaderStructureCPUVertexPositionNormalTexture
{
    ShaderStructureCPUVertexPositionNormalTexture()
    { 
    
    }

    ShaderStructureCPUVertexPositionNormalTexture(const XMFLOAT3& position, const XMFLOAT3& normal, const XMFLOAT2& textureCoordinate)
        : position(position),
          normal(normal),
          textureCoordinate(textureCoordinate)
    { 
    
    }

    ShaderStructureCPUVertexPositionNormalTexture(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
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

// Used to send per-vertex data to the vertex shader.
// Vertex struct holding position, normal vector, and texture mapping information.
struct ShaderStructureCPUVertexPositionNormalColor
{
    ShaderStructureCPUVertexPositionNormalColor()
    {

    }

    ShaderStructureCPUVertexPositionNormalColor(const XMFLOAT3& position, const XMFLOAT3& normal, const XMFLOAT3& color)
        : position(position),
        normal(normal),
        color(color)
    {

    }

    ShaderStructureCPUVertexPositionNormalColor(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR color)
    {
        XMStoreFloat3(&this->position, position);
        XMStoreFloat3(&this->normal, normal);
        XMStoreFloat3(&this->color, color);
    }

    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT3 color;

    static const int input_element_count = 3;
    static const D3D12_INPUT_ELEMENT_DESC input_elements[input_element_count];
};

// Used to send per-vertex data to the vertex shader.
struct ShaderStructureCPUVertexPositionColor
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 color;
};

struct ShaderStructureCPUModelAndInverseTransposeModelView
{
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 inverse_transpose_model_view;
};

class Mesh
{
    public:
        Mesh(bool isStatic);
    
        void InitializeAsCube(float size = 1);
        void InitializeAsSphere(float diameter = 1, size_t tessellation = 16);
        void InitializeAsCone(float diameter = 1, float height = 1, size_t tessellation = 32);
        void InitializeAsTorus(float diameter = 1, float thickness = 0.333f, size_t tessellation = 32);
        void InitializeAsPlane(float width = 1, float height = 1);
        static void ReverseWinding(vector<uint16_t>& indices, vector<ShaderStructureCPUVertexPositionNormalTexture>& vertices);

        void SetIsStatic(bool isStatic);

        vector<ShaderStructureCPUVertexPositionNormalTexture> vertices;
        vector<uint16_t> indices;

        XMFLOAT3 world_position;
        XMFLOAT3 local_rotation;
        
        bool is_static;

    private:
        Mesh();
        friend class SceneRenderer3D;
        XMMATRIX transform_matrix;

        Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer_upload;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

        Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer_upload;
        D3D12_INDEX_BUFFER_VIEW index_buffer_view;

        UINT64 fence_value_signaling_vertex_and_index_bufferr_residency;
        bool vertex_and_index_buffer_upload_started;

        // The first index tells us in which out of all the buffers reserved for that frame contains this object's model transform matrix
        // and the second index tells us the offset inside of that buffer.
        UINT per_frame_transform_matrix_buffer_indexes[c_frame_count][2];
        bool per_frame_transform_matrix_buffer_indexes_assigned[c_frame_count];
        UINT current_frame_index_containing_most_updated_transform_matrix;
        // If the mesh is initialized as static, it will require one model transform matrix update in order for it to be renderer at the
        // right place. When initialized as static, the mesh will have the public is_static bool set to false
        // and this private initialized_as_statis bool set to true. This bool overrides the public is_static bool for only one frame, 
        // that way even though the is_static bool is set to true, which would skip the model transform matrix update for this mesh
        // this initialized_as_statis bool forces one update.
        bool initialized_as_static;
        // Computes a point on a unit circle, aligned to the x/z plane and centered on the origin.
        inline XMVECTOR GetCircleVector(size_t i, size_t tessellation);
        XMVECTOR GetCircleTangent(size_t i, size_t tessellation);
        // Helper creates a triangle fan to close the end of a cylinder / cone
        void CreateCylinderCap(vector<ShaderStructureCPUVertexPositionNormalTexture>& vertices, vector<uint16_t>& indices, size_t tessellation, float height, float radius, bool isTop);

        
};