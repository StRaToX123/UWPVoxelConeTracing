#pragma once

#include "Graphics/DeviceResources/DeviceResources.h"
#include "Utility/Math/Matrix.h"
#include "Scene/Camera.h"
#include <DirectXColors.h>


using namespace DirectX;



struct ShaderStructureCPUSpotLight
{
    ShaderStructureCPUSpotLight() :
        position_world_space(0.0f, 0.0f, 0.0f, 1.0f),
        position_view_space(0.0f, 0.0f, 0.0f, 1.0f),
        direction_world_space(0.0f, 0.0f, 1.0f, 1.0f),
        direction_view_space(0.0f, 0.0f, 1.0f, 1.0f),
        color(1.0f, 1.0f, 1.0f, 1.0f),
        intensity(1.0f),
        spot_angle_degrees(90.0f),
        spot_angle_radians(XMConvertToRadians(90.0f)),
        spot_half_angle_degrees(45.0f),
        spot_half_angle_radians(XMConvertToRadians(45.0f)),
        attenuation(0.0f)
    {

    }

    void UpdatePositionViewSpace(Camera& camera)
    {
        XMStoreFloat4(&position_view_space, XMVector4Transform(XMVectorSet(position_world_space.x, position_world_space.y, position_world_space.z, 1.0f), camera.GetViewMatrix()));
    }


    void UpdateDirectionViewSpace(Camera& camera)
    {
        // First check if there are any unallowed values set for some of the constant buffer variables
        if ((direction_world_space.x + direction_world_space.y + direction_world_space.z) == 0.0f)
        {
            direction_world_space.y = -1.0f;
        }
        
        XMStoreFloat4(&direction_view_space, XMVector3Normalize(XMVector4Transform(XMVectorSet(direction_world_space.x, direction_world_space.y, direction_world_space.z, 1.0f), DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationQuaternion(camera.GetRotationQuaternion())))));
    };

    void UpdateSpotLightViewMatrix()
    {
        XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(-position_world_space.x, -position_world_space.y, -position_world_space.z);
        XMVECTOR rotationQuaternion = XMQuaternionRotationMatrix(DirectX::XMMatrixTranspose(DirectX::XMMatrixLookToLH(XMVectorSet(position_world_space.x, position_world_space.y, position_world_space.z, 1.0f), XMVectorSet(direction_world_space.x, direction_world_space.y, direction_world_space.z, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f))));
        XMMATRIX rotationMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationQuaternion(rotationQuaternion));
        DirectX::XMStoreFloat4x4(&spotlight_view_matrix, DirectX::XMMatrixTranspose(translationMatrix * rotationMatrix));
    }

    void UpdateSpotLightProjectionMatrix()
    {
        // First check if there are any unallowed values set for some of the constant buffer variables
        if (spot_angle_degrees == 0.0f)
        {
            spot_angle_degrees = 1.0f;
        }

        spot_angle_radians = XMConvertToRadians(spot_angle_degrees);
        spot_half_angle_degrees = spot_angle_degrees / 2.0f;
        spot_half_angle_radians = XMConvertToRadians(spot_half_angle_degrees);
        XMStoreFloat4x4(&spotlight_projection_matrix, DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(spot_angle_radians, 1.0f, 0.1f, 100.0f)));
    }


    XMFLOAT4    position_world_space;
    XMFLOAT4    position_view_space;
    XMFLOAT4    direction_world_space;
    XMFLOAT4    direction_view_space;
    XMFLOAT4    color;
    float       intensity;
    float       spot_angle_degrees;
    float       spot_angle_radians;
    float       spot_half_angle_degrees;
    float       spot_half_angle_radians;
    float       attenuation;
    XMFLOAT2    padding;
    XMFLOAT4X4  spotlight_view_matrix;
    XMFLOAT4X4  spotlight_projection_matrix;
};

class SpotLight
{
    public:
        SpotLight();
        ~SpotLight();
        void Initialize(const WCHAR* name, UINT64 shadowMapWidth, UINT shadowMapHeight, const std::shared_ptr<DeviceResources>& deviceResources);
        // Copy 2 descripts, one for the CBV light data and another for the SRV shadow map texture
        // Optionally copys one more and that is the depth buffer's SRV if the copyDepthBufferSRV bool is true.
        void CopyDescriptorsIntoDescriptorHeap(CD3DX12_CPU_DESCRIPTOR_HANDLE& destinationDescriptorHandle, bool copyShadowMapSRV = false, bool copyDepthBufferSRV = false);
        void AssignDescriptors(ID3D12GraphicsCommandList* _commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE& descriptorHandle, UINT rootParameterIndex, bool assignCompute = false);
        void UpdateConstantBuffers();
        void UpdateShadowMapBuffers(UINT64 shadowMapWidth, UINT shadowMapHeight);
        D3D12_CPU_DESCRIPTOR_HANDLE GetShadowMapRenderTargetView(); 
        D3D12_CPU_DESCRIPTOR_HANDLE GetShadowMapDepthStencilView();
        ID3D12Resource* GetShadowMapDepthBuffer() const { return depth_buffer_shadow_map.Get(); };
        ID3D12Resource* GetShadowMapBuffer() const { return texture_shadow_map.Get(); };
        D3D12_VIEWPORT GetViewport() const { return viewport; };
        D3D12_RECT GetScissorRect() const { return scissor_rect; };
        

        const WCHAR* pName;
        ShaderStructureCPUSpotLight constant_buffer_data;
        Camera camera;
    private:
        
        bool is_initialized;
        std::shared_ptr<DeviceResources> device_resources;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_cbv_srv_uav;
        
        static const UINT c_aligned_constant_buffer_data = (sizeof(ShaderStructureCPUSpotLight) + 255) & ~255;
        
        Microsoft::WRL::ComPtr<ID3D12Resource> resource_constant_buffer_data;
        UINT8* constant_mapped_buffer_data;
        UINT8 most_updated_constant_buffer_index;

        Microsoft::WRL::ComPtr<ID3D12Resource> texture_shadow_map;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap;
        Microsoft::WRL::ComPtr<ID3D12Resource> depth_buffer_shadow_map;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap;
        D3D12_VIEWPORT viewport;
        D3D12_RECT scissor_rect;
        
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
