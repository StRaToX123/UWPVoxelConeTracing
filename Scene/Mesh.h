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
        Mesh(bool isStatic);
    
        void InitializeAsCube(float size = 1);
        void InitializeAsSphere(float diameter = 1, size_t tessellation = 16);
        void InitializeAsCone(float diameter = 1, float height = 1, size_t tessellation = 32);
        void InitializeAsTorus(float diameter = 1, float thickness = 0.333f, size_t tessellation = 32);
        void InitializeAsPlane(float width = 1, float height = 1);
        static void ReverseWinding(vector<uint16_t>& indices, vector<VertexPositionNormalTexture>& vertices);

        void SetIsStatic(bool isStatic);

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

        UINT64 fence_value_signaling_required_resource_residency;
        bool vertex_and_index_buffer_upload_started;

        UINT64 fence_value_signaling_model_transform_matrix_residency;
        // We need to skip rendering the object for the frame it is shown,
        // because it's model transform matrix isn't uploaded to the gpu yet
        bool skip_rendering_this_object_for_its_first_frame;
        // The first index tells us in which out of all the buffers reserved for that frame contains this object's model transform matrix
        // and the second index tells us the offset inside of that buffer.
        vector<UINT> per_frame_model_transform_matrix_buffer_indexes[c_frame_count];
        bool per_frame_model_transform_matrix_buffer_indexes_assigned[c_frame_count];
        // Since each object's model transfom matrix comes in pairs of two (in order to have one be readable and the other
        // writable for updates), we will need to switch which pair we are reading and writing to each time this object's
        // model transform matrix needs to be updated. This happens on a per frame basis. This index switch direction
        // tells us which if out subresource matrix copy pair is offset in the positive or the negative direction 
        //int per_frame_model_transform_matrix_buffer_indexes_switch_direction[c_frame_count];
        //bool per_frame_model_transform_matrix_buffer_indexes_require_update[c_frame_count];
        UINT frame_index_containing_most_updated_model_transform_matrix;
        bool is_static;
        
        
        

    private:
        friend class Sample3DSceneRenderer;
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
        void CreateCylinderCap(vector<VertexPositionNormalTexture>& vertices, vector<uint16_t>& indices, size_t tessellation, float height, float radius, bool isTop);

        
};