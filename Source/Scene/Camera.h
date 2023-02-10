#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include "Graphics\DirectX\DX12Buffer.h"



// When performing transformations on the camera, 
// it is sometimes useful to express which space this 
// transformation should be applied.
enum class Space
{
    Local,
    World,
};

class Camera
{
    public:
        Camera();
        ~Camera();

        void Initialize(DX12DescriptorHeapManager* _descriptorHeapManager);
        //void XM_CALLCONV SetLookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up );
        DirectX::XMMATRIX GetViewMatrix();
        void SetProjection(float fovy, float aspect, float zNear, float zFar);
        DirectX::XMMATRIX GetProjectionMatrix();
        void SetFoVYDeggrees(float fovyInDegrees);
        float GetFoVYDegrees() const;
        void XM_CALLCONV SetPositionWorldSpace(DirectX::XMVECTOR positionWorldSpace);
        DirectX::XMVECTOR GetPositionWorldSpace() const;
        void XM_CALLCONV SetRotationLocalSpaceQuaternion(DirectX::FXMVECTOR rotationLocalSpaceQuaternion);
        DirectX::XMVECTOR GetRotationLocalSpaceQuaternion() const;
        void XM_CALLCONV Translate(DirectX::FXMVECTOR translation, Space space = Space::Local);
        //void Rotate(DirectX::FXMVECTOR quaternion);
        void UpdateBuffers();
        DX12Buffer* GetCB() const { return p_constant_buffer; };

        struct ShaderStructureCPUCamera
        {
            DirectX::XMFLOAT4X4 view_projection;
            DirectX::XMFLOAT3 position_world_space;
            float padding;
            //DirectX::XMFLOAT4X4 view;
            //DirectX::XMFLOAT4X4 projection;
            //DirectX::XMFLOAT4X4 inverse_view;
        };
    private:
        void UpdateViewMatrix();
        void UpdateProjectionMatrix();

        bool is_initialized;

        XMVECTOR position_world_space;
        XMVECTOR rotation_local_space_quaternion;
        XMMATRIX view_matrix;
        XMMATRIX projection_matrix;
        // projection parameters
        float vertical_fov_degrees;   // Vertical field of view.
        float aspect_ratio; // Aspect ratio
        float z_near;      // Near clip distance
        float z_far;       // Far clip distance.
        // True if the view matrix needs to be updated.
        bool is_dirty_view_matrix;
        // True if the projection matrix needs to be updated.
        bool is_dirty_projection_matrix;

        DX12Buffer* p_constant_buffer;
        static const UINT c_aligned_shader_structure_cpu_camera = (sizeof(ShaderStructureCPUCamera) + 255) & ~255;
        ShaderStructureCPUCamera shader_structure_cpu_camera_data;
};