#include "Graphics\SceneRenderers\SceneRendererDirectLightingVoxelGIandAO.h"


static const float gs_clear_color_black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const float gs_clear_color_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

SceneRendererDirectLightingVoxelGIandAO::SceneRendererDirectLightingVoxelGIandAO()
{
	p_constant_buffer_voxelization = nullptr;
	p_constant_buffer_vct_main = nullptr;
	p_constant_buffer_lighting = nullptr;
	p_constant_buffer_illumination_flags = nullptr;


	p_render_target_vct_voxelization = nullptr;
	p_render_target_vct_main = nullptr;
	p_render_target_vct_main_upsample_and_blur = nullptr;
	p_render_target_lighting = nullptr;
	
	p_depth_buffer_shadow_mapping = nullptr;
	p_append_buffer_indirect_draw_voxel_debug_per_instance_data = nullptr;
	p_voxel_debug_cube = nullptr;
	p_counter_reset_buffer = nullptr;
	p_indirect_command_buffer_voxel_debug = nullptr;
	p_render_target_vct_debug = nullptr;
}

SceneRendererDirectLightingVoxelGIandAO::~SceneRendererDirectLightingVoxelGIandAO()
{
	if (p_render_target_vct_debug != nullptr)
	{
		delete p_render_target_vct_debug;
	}

	if (p_indirect_command_buffer_voxel_debug != nullptr)
	{
		delete p_indirect_command_buffer_voxel_debug;
	}

	if (p_counter_reset_buffer != nullptr)
	{
		delete p_counter_reset_buffer;
	}

	if (p_append_buffer_indirect_draw_voxel_debug_per_instance_data != nullptr)
	{
		delete p_append_buffer_indirect_draw_voxel_debug_per_instance_data;
	}

	if (p_voxel_debug_cube != nullptr)
	{
		delete p_voxel_debug_cube;
	}

	if (p_constant_buffer_voxelization != nullptr)
	{
		delete p_constant_buffer_voxelization;
	}

	if (p_constant_buffer_vct_main != nullptr)
	{
		delete p_constant_buffer_vct_main;
	}

	if (p_constant_buffer_lighting != nullptr)
	{
		delete p_constant_buffer_lighting;
	}

	if (p_constant_buffer_illumination_flags != nullptr)
	{
		delete p_constant_buffer_illumination_flags;
	}

	if (p_render_target_vct_voxelization != nullptr)
	{
		delete p_render_target_vct_voxelization;
	}

	if (p_render_target_vct_main != nullptr)
	{
		delete p_render_target_vct_main;
	}

	if (p_render_target_vct_main_upsample_and_blur != nullptr)
	{
		delete p_render_target_vct_main_upsample_and_blur;
	}

	if (p_render_target_lighting != nullptr)
	{
		delete p_render_target_lighting;
	}

	if (p_depth_buffer_shadow_mapping != nullptr)
	{
		delete p_depth_buffer_shadow_mapping;
	}

	if (P_constant_buffers_vct_anisotropic_mip_generation.size() > 0)
	{
		for (UINT i = 0; i < P_constant_buffers_vct_anisotropic_mip_generation.size(); i++)
		{
			delete P_constant_buffers_vct_anisotropic_mip_generation[i];
		}
	}

	if (P_render_targets_vct_voxelization_anisotropic_mip_generation.size() > 0)
	{
		for (UINT i = 0; i < P_render_targets_vct_voxelization_anisotropic_mip_generation.size(); i++)
		{
			delete P_render_targets_vct_voxelization_anisotropic_mip_generation[i];
		}
	}
	
	if (P_render_targets_gbuffer.size() > 0)
	{
		for (UINT i = 0; i < P_render_targets_gbuffer.size(); i++)
		{
			delete P_render_targets_gbuffer[i];
		}
	}
}

void SceneRendererDirectLightingVoxelGIandAO::Initialize(Windows::UI::Core::CoreWindow^ coreWindow, ID3D12GraphicsCommandList* _commandList)
{
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	descriptor_heap_manager.Initialize();

	ID3D12Device* _d3dDevice = _deviceResources->GetD3DDevice();

	fence_unused_value_direct_queue = 1;
	ThrowIfFailed(_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_direct_queue)));
	fence_unused_value_compute_queue = 1;
	ThrowIfFailed(_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_compute_queue)));
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
	
	auto size = _deviceResources->GetOutputSize();
	float aspectRatio = float(size.right) / float(size.bottom);

	#pragma region States
	depth_state_read_write.DepthEnable = TRUE;
	depth_state_read_write.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depth_state_read_write.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depth_state_read_write.StencilEnable = FALSE;
	depth_state_read_write.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_state_read_write.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depth_state_read_write.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depth_state_read_write.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depth_state_read_write.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depth_state_read_write.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depth_state_read_write.BackFace = depth_state_read_write.FrontFace;

	depth_state_disabled.DepthEnable = FALSE;
	depth_state_disabled.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth_state_disabled.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depth_state_disabled.StencilEnable = FALSE;
	depth_state_disabled.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_state_disabled.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depth_state_disabled.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depth_state_disabled.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depth_state_disabled.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depth_state_disabled.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depth_state_disabled.BackFace = depth_state_read_write.FrontFace;

	blend_state_default.IndependentBlendEnable = FALSE;
	blend_state_default.RenderTarget[0].BlendEnable = FALSE;
	blend_state_default.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend_state_default.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend_state_default.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend_state_default.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blend_state_default.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blend_state_default.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend_state_default.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	rasterizer_state_default.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer_state_default.CullMode = D3D12_CULL_MODE_BACK;
	rasterizer_state_default.FrontCounterClockwise = FALSE;
	rasterizer_state_default.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizer_state_default.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer_state_default.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizer_state_default.DepthClipEnable = TRUE;
	rasterizer_state_default.MultisampleEnable = FALSE;
	rasterizer_state_default.AntialiasedLineEnable = FALSE;
	rasterizer_state_default.ForcedSampleCount = 0;
	rasterizer_state_default.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	rasterizer_state_no_cull_no_depth.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer_state_no_cull_no_depth.CullMode = D3D12_CULL_MODE_NONE;
	rasterizer_state_no_cull_no_depth.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizer_state_no_cull_no_depth.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer_state_no_cull_no_depth.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizer_state_no_cull_no_depth.DepthClipEnable = FALSE;
	rasterizer_state_no_cull_no_depth.MultisampleEnable = FALSE;
	rasterizer_state_no_cull_no_depth.AntialiasedLineEnable = FALSE;
	rasterizer_state_no_cull_no_depth.ForcedSampleCount = 0;
	rasterizer_state_no_cull_no_depth.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	rasterizer_state_shadow_mapping.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer_state_shadow_mapping.CullMode = D3D12_CULL_MODE_BACK;
	rasterizer_state_shadow_mapping.FrontCounterClockwise = FALSE;
	rasterizer_state_shadow_mapping.SlopeScaledDepthBias = 10.0f;
	rasterizer_state_shadow_mapping.DepthBias = 0.05f;
	rasterizer_state_shadow_mapping.DepthClipEnable = FALSE;
	rasterizer_state_shadow_mapping.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer_state_shadow_mapping.MultisampleEnable = FALSE;
	rasterizer_state_shadow_mapping.AntialiasedLineEnable = FALSE;
	rasterizer_state_shadow_mapping.ForcedSampleCount = 0;
	rasterizer_state_shadow_mapping.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	sampler_bilinear.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_bilinear.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler_bilinear.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler_bilinear.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler_bilinear.MipLODBias = 0;
	sampler_bilinear.MaxAnisotropy = 16;
	sampler_bilinear.MinLOD = 0.0f;
	sampler_bilinear.MaxLOD = D3D12_FLOAT32_MAX;
	sampler_bilinear.BorderColor[0] = 0.0f;
	sampler_bilinear.BorderColor[1] = 0.0f;
	sampler_bilinear.BorderColor[2] = 0.0f;
	sampler_bilinear.BorderColor[3] = 0.0f;
	#pragma endregion

	// Set the most updated indexes for the constant buffers
	shader_structure_cpu_vct_main_data_most_updated_index = _deviceResources->GetBackBufferCount() - 1;
	shader_structure_cpu_illumination_flags_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;
	shader_structure_cpu_voxelization_data_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;
	shader_structure_cpu_lighting_data_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;
	shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;

	InitGbuffer(_d3dDevice, coreWindow->Bounds.Width, coreWindow->Bounds.Height);
	InitShadowMapping(_d3dDevice, coreWindow->Bounds.Width, coreWindow->Bounds.Height);
	InitVoxelConeTracing(_deviceResources, _d3dDevice, coreWindow->Bounds.Width, coreWindow->Bounds.Height, _commandList);
	InitLighting(_deviceResources, _d3dDevice, coreWindow->Bounds.Width, coreWindow->Bounds.Height);
	InitComposite(_deviceResources, _d3dDevice);
	UpdateBuffers(UpdatableBuffers::ILLUMINATION_FLAGS_DATA_BUFFER);
	UpdateBuffers(UpdatableBuffers::VCT_MAIN_DATA_BUFFER);
	UpdateBuffers(UpdatableBuffers::VOXELIZATION_DATA_BUFFER);
	UpdateBuffers(UpdatableBuffers::LIGHTING_DATA_BUFFER);
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateBuffers(UpdatableBuffers whichBufferToUpdate)
{
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	switch (whichBufferToUpdate)
	{
		case UpdatableBuffers::ILLUMINATION_FLAGS_DATA_BUFFER:
		{
			shader_structure_cpu_illumination_flags_most_updated_index++;
			if (shader_structure_cpu_illumination_flags_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_illumination_flags_most_updated_index = 0;
			}

			memcpy(p_constant_buffer_illumination_flags->GetMappedData(shader_structure_cpu_illumination_flags_most_updated_index), &shader_structure_cpu_illumination_flags_data, sizeof(ShaderStructureCPUIlluminationFlagsData));
			break;
		}

		case UpdatableBuffers::VCT_MAIN_DATA_BUFFER:
		{
			shader_structure_cpu_vct_main_data_most_updated_index++;
			if (shader_structure_cpu_vct_main_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_vct_main_data_most_updated_index = 0;
			}

			shader_structure_cpu_vct_main_data.upsample_ratio = DirectX::XMFLOAT2(P_render_targets_gbuffer[0]->GetWidth() / p_render_target_vct_main->GetWidth(), P_render_targets_gbuffer[0]->GetHeight() / p_render_target_vct_main->GetHeight());
			memcpy(p_constant_buffer_vct_main->GetMappedData(shader_structure_cpu_vct_main_data_most_updated_index), &shader_structure_cpu_vct_main_data, sizeof(ShaderStructureCPUVCTMainData));
			break;
		}

		case UpdatableBuffers::VOXELIZATION_DATA_BUFFER:
		{
			shader_structure_cpu_voxelization_data_most_updated_index++;
			if (shader_structure_cpu_voxelization_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_voxelization_data_most_updated_index = 0;
			}

			shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.x = -(shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space * 0.5f);
			shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.y = -shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.x;
			shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.z = shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.x;
			shader_structure_cpu_voxelization_data.voxel_scale = shader_structure_cpu_voxelization_data.voxel_grid_res * 0.5f;
			shader_structure_cpu_voxelization_data.voxel_extent = shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space / shader_structure_cpu_voxelization_data.voxel_grid_res;
			shader_structure_cpu_voxelization_data.voxel_extent_rcp = 1.0f / (shader_structure_cpu_voxelization_data.voxel_extent);
			shader_structure_cpu_voxelization_data.voxel_half_extent = shader_structure_cpu_voxelization_data.voxel_extent * 0.5f;
			shader_structure_cpu_voxelization_data.voxel_grid_half_extent_world_space_rcp = 1.0f / (shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space * 0.5f);
			// For the mip count we don't want to count the 1x1x1 mip since the anisotropic mip chain generation 
			// requires at least a 2x2x2 texture in order to perform calculations
			// We also don't want to count the mip level 0 (full res mip) since the anisotropic mip chain starts from what would
			// be the original 3D textures mip level 1
			shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count = (UINT32)log2(shader_structure_cpu_voxelization_data.voxel_grid_res) - 1;
			memcpy(p_constant_buffer_voxelization->GetMappedData(shader_structure_cpu_voxelization_data_most_updated_index), &shader_structure_cpu_voxelization_data, sizeof(ShaderStructureCPUVoxelizationData));
			break;
		}

		case UpdatableBuffers::LIGHTING_DATA_BUFFER:
		{
			shader_structure_cpu_lighting_data_most_updated_index++;
			if (shader_structure_cpu_lighting_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_lighting_data_most_updated_index = 0;
			}

			shader_structure_cpu_lighting_data.shadow_texel_size = XMFLOAT2(1.0f / p_depth_buffer_shadow_mapping->GetWidth(), 1.0f / p_depth_buffer_shadow_mapping->GetHeight());
			memcpy(p_constant_buffer_lighting->GetMappedData(shader_structure_cpu_lighting_data_most_updated_index), &shader_structure_cpu_lighting_data, sizeof(ShaderStructureCPUVoxelizationData));
			break;
		}
	}
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateGBuffer(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	if (P_render_targets_gbuffer.size() != 0)
	{
		for (UINT i = 0; i < P_render_targets_gbuffer.size(); i++)
		{
			delete P_render_targets_gbuffer[i];
		}
	}

	P_render_targets_gbuffer.clear();
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	P_render_targets_gbuffer.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, backBufferWidth, backBufferHeight, format, flags, L"Albedo"));
	format = DXGI_FORMAT_R16G16B16A16_SNORM;
	P_render_targets_gbuffer.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, backBufferWidth, backBufferHeight, format, flags, L"Normals"));
	format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	P_render_targets_gbuffer.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, backBufferWidth, backBufferHeight, format, flags, L"World Positions"));
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateShadowMappingBuffers(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	if (p_depth_buffer_shadow_mapping != nullptr)
	{
		delete p_depth_buffer_shadow_mapping;
	}
	
	p_depth_buffer_shadow_mapping = new DX12DepthBuffer(_d3dDevice, &descriptor_heap_manager, SHADOWMAP_SIZE, SHADOWMAP_SIZE, DXGI_FORMAT_D32_FLOAT);
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateVoxelConeTracingVoxelizationBuffers(ID3D12Device* _d3dDevice)
{
	if (p_render_target_vct_voxelization != nullptr)
	{
		delete p_render_target_vct_voxelization;
	}

	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	p_render_target_vct_voxelization = new DX12RenderTarget(_d3dDevice,
		&descriptor_heap_manager,
		shader_structure_cpu_voxelization_data.voxel_grid_res,
		shader_structure_cpu_voxelization_data.voxel_grid_res,
		format,
		flags,
		L"Voxelization Scene Data 3D",
		shader_structure_cpu_voxelization_data.voxel_grid_res,
		1,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	if (P_render_targets_vct_voxelization_anisotropic_mip_generation.size() != 0)
	{
		for (UINT i = 0; i < P_render_targets_vct_voxelization_anisotropic_mip_generation.size(); i++)
		{
			delete P_render_targets_vct_voxelization_anisotropic_mip_generation[i];
		}
	}

	P_render_targets_vct_voxelization_anisotropic_mip_generation.clear();
	format = DXGI_FORMAT_R8G8B8A8_UNORM;
	flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	int size = shader_structure_cpu_voxelization_data.voxel_grid_res >> 1;
	P_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, size, size, format, flags, L"Voxelization Scene Mip Prepare X+ 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	P_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, size, size, format, flags, L"Voxelization Scene Mip Prepare X- 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	P_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, size, size, format, flags, L"Voxelization Scene Mip Prepare Y+ 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	P_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, size, size, format, flags, L"Voxelization Scene Mip Prepare Y- 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	P_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, size, size, format, flags, L"Voxelization Scene Mip Prepare Z+ 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	P_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, size, size, format, flags, L"Voxelization Scene Mip Prepare Z- 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateVoxelConeTracingBuffers(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	if (p_render_target_vct_main != nullptr)
	{
		delete p_render_target_vct_main;
	}
	
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	p_render_target_vct_main = new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, backBufferWidth * mVCTRTRatio, backBufferHeight * mVCTRTRatio, format, flags, L"VCT Final Output", -1, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	if (p_render_target_vct_main_upsample_and_blur != nullptr)
	{
		delete p_render_target_vct_main_upsample_and_blur;
	}
	
	p_render_target_vct_main_upsample_and_blur = new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, backBufferWidth, backBufferHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"VCT Main RT Upsampled & Blurred", -1, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateLightingBuffers(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	if (p_render_target_lighting != nullptr)
	{
		delete p_render_target_lighting;
	}

	p_render_target_lighting = new DX12RenderTarget(_d3dDevice, &descriptor_heap_manager, backBufferWidth, backBufferHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Lighting");
}

void SceneRendererDirectLightingVoxelGIandAO::OnWindowSizeChanged(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	UpdateGBuffer(_d3dDevice, backBufferWidth, backBufferHeight);
	UpdateShadowMappingBuffers(_d3dDevice, backBufferWidth, backBufferHeight);
	UpdateVoxelConeTracingBuffers(_d3dDevice, backBufferWidth, backBufferHeight);
	UpdateLightingBuffers(_d3dDevice, backBufferWidth, backBufferHeight);
}

// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
// This function expects to be given an already reset direct command list and a an already reset compute command list.
// When returning the functions leaves the direct command list in a reset state and the compute command list in an unreset state
// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
void SceneRendererDirectLightingVoxelGIandAO::Render(std::vector<Model*>& scene,
	DirectionalLight& directionalLight,
	DXRSTimer& mTimer,
	Camera& camera,
	D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView,
	ID3D12GraphicsCommandList* _commandListDirect,
	ID3D12GraphicsCommandList* _commandListCompute,
	ID3D12CommandAllocator* _commandAllocatorDirect)
{	
	// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
	// This function expects to be given an already reset direct command list and a an already reset compute command list.
	// When returning the functions leaves the direct command list in a reset state and the compute command list in an unreset state
	// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	ID3D12CommandList* __commandListsDirect[] = { _commandListDirect };
	ID3D12CommandList* __commandListsCompute[] = { _commandListCompute };
	auto _d3dDevice = _deviceResources->GetD3DDevice();
	DX12DescriptorHeapGPU* gpuDescriptorHeap = descriptor_heap_manager.GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescriptorHeap->Reset();
	ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };
	
	DX12DescriptorHandleBlock modelDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock(scene.size());
	for (int i = 0; i < scene.size(); i++)
	{
		modelDataDescriptorHandleBlock.Add(scene[i]->GetCBV());
	}

	DX12DescriptorHandleBlock cameraDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
	cameraDataDescriptorHandleBlock.Add(camera.GetCBV());

	DX12DescriptorHandleBlock directionalLightDataDescriptorHeapBlock = gpuDescriptorHeap->GetHandleBlock();
	directionalLightDataDescriptorHeapBlock.Add(directionalLight.GetCBV());

	DX12DescriptorHandleBlock shadowDepthDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
	shadowDepthDescriptorHandleBlock.Add(p_depth_buffer_shadow_mapping->GetSRV());

	// Graphics command list
	{
		_commandListDirect->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		_commandListDirect->ClearDepthStencilView(_deviceResources->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		RenderGbuffer(_deviceResources, _d3dDevice, _commandListDirect, gpuDescriptorHeap, scene, modelDataDescriptorHandleBlock, cameraDataDescriptorHandleBlock);
		RenderShadowMapping(_deviceResources, _d3dDevice, _commandListDirect, gpuDescriptorHeap, scene, modelDataDescriptorHandleBlock, directionalLightDataDescriptorHeapBlock);
		RenderVoxelConeTracing(_deviceResources, 
			_d3dDevice, 
			_commandListDirect, 
			gpuDescriptorHeap, 
			scene, 
			modelDataDescriptorHandleBlock, 
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock,
			GRAPHICS_QUEUE); // Only voxelization there which can't go to compute

		// We have to transition the GBuffers here inside of the graphics command list to a non pixel shader resource,
		// because the compute command list cannot transition resources from a state of a render target
		_deviceResources->ResourceBarriersBegin(barriers);
		P_render_targets_gbuffer[0]->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		P_render_targets_gbuffer[1]->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		P_render_targets_gbuffer[2]->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_deviceResources->ResourceBarriersEnd(barriers, _commandListDirect);

		_commandListDirect->Close();
		_deviceResources->GetCommandQueueDirect()->ExecuteCommandLists(1, __commandListsDirect);
		_deviceResources->GetCommandQueueDirect()->Signal(fence_direct_queue.Get(), fence_unused_value_direct_queue);
		fence_unused_value_direct_queue++;
	}

	// Compute command list
	{
		_commandListCompute->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		RenderVoxelConeTracing(_deviceResources,
			_d3dDevice,
			_commandListCompute,
			gpuDescriptorHeap,
			scene,
			modelDataDescriptorHandleBlock,
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock,
			COMPUTE_QUEUE);

		_deviceResources->GetCommandQueueCompute()->Wait(fence_direct_queue.Get(), fence_unused_value_direct_queue - 1);
		_commandListCompute->Close();
		_deviceResources->GetCommandQueueCompute()->ExecuteCommandLists(1, __commandListsCompute);
		_deviceResources->GetCommandQueueCompute()->Signal(fence_compute_queue.Get(), fence_unused_value_compute_queue);
		fence_unused_value_compute_queue++;
	}

	// Submit the rest of the frame to graphics command list
	{
		_commandListDirect->Reset(_commandAllocatorDirect, nullptr);
		_commandListDirect->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		RenderLighting(_deviceResources,
			_d3dDevice,
			_commandListDirect,
			gpuDescriptorHeap,
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock,
			fullScreenQuadVertexBufferView);
		RenderComposite(_deviceResources, 
			_d3dDevice, 
			_commandListDirect, 
			gpuDescriptorHeap, 
			fullScreenQuadVertexBufferView);

		// We need to transition these resources here inside of the graphics command list,
		// since the compute command list will not allow us to transition a resource 
		// from a pixel shader resource state.
		_deviceResources->ResourceBarriersBegin(barriers);
		p_render_target_vct_main->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_COMMON);
		p_render_target_vct_main_upsample_and_blur->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_COMMON);
		_deviceResources->ResourceBarriersEnd(barriers, _commandListDirect);

		_deviceResources->GetCommandQueueDirect()->Wait(fence_compute_queue.Get(), fence_unused_value_compute_queue - 1);
	}
}

void SceneRendererDirectLightingVoxelGIandAO::ThrowFailedErrorBlob(ID3DBlob* blob)
{
	std::string message = "";
	if (blob)
	{
		message.append((char*)blob->GetBufferPointer());
	}

	throw std::runtime_error(message.c_str());
}

void SceneRendererDirectLightingVoxelGIandAO::InitGbuffer(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	D3D12_SAMPLER_DESC sampler;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;

	UpdateGBuffer(_d3dDevice, backBufferWidth, backBufferHeight);

	// Root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	root_signature_gbuffer.Reset(2, 1);
	root_signature_gbuffer.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_gbuffer[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_gbuffer[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_gbuffer.Finalize(_d3dDevice, L"GPrepassRS", rootSignatureFlags);

	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("GBuffer_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode(GetFilePath("GBuffer_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Describe and create the graphics pipeline state object (PSO).
	DXGI_FORMAT formats[3];
	formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	formats[1] = DXGI_FORMAT_R16G16B16A16_SNORM;
	formats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;

	pipeline_state_gbuffer.SetRootSignature(root_signature_gbuffer);
	pipeline_state_gbuffer.SetRasterizerState(rasterizer_state_default);
	pipeline_state_gbuffer.SetBlendState(blend_state_default);
	pipeline_state_gbuffer.SetDepthStencilState(depth_state_read_write);
	pipeline_state_gbuffer.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	pipeline_state_gbuffer.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipeline_state_gbuffer.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
	pipeline_state_gbuffer.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	pipeline_state_gbuffer.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	pipeline_state_gbuffer.Finalize(_d3dDevice);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
}
void SceneRendererDirectLightingVoxelGIandAO::RenderGbuffer(DX12DeviceResourcesSingleton* _deviceResources,
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap, 
	std::vector<Model*>& scene,
	DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	{
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);

		commandList->SetPipelineState(pipeline_state_gbuffer.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(root_signature_gbuffer.GetSignature());

		// Transition buffers to rendertarget outputs
		_deviceResources->ResourceBarriersBegin(barriers);
		P_render_targets_gbuffer[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		P_render_targets_gbuffer[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		P_render_targets_gbuffer[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		_deviceResources->ResourceBarriersEnd(barriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
		{
			P_render_targets_gbuffer[0]->GetRTV(),
			P_render_targets_gbuffer[1]->GetRTV(),
			P_render_targets_gbuffer[2]->GetRTV(),
		};

		commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &_deviceResources->GetDepthStencilView());
		commandList->ClearRenderTargetView(rtvHandles[0], gs_clear_color_black, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[1], gs_clear_color_black, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[2], gs_clear_color_black, 0, nullptr);
		
		commandList->SetGraphicsRootDescriptorTable(0, cameraDataDescriptorHandleBlock.GetGPUHandle());
		CD3DX12_GPU_DESCRIPTOR_HANDLE modelDataGPUdescriptorHandle = modelDataDescriptorHandleBlock.GetGPUHandle();
		for (UINT i = 0; i < scene.size(); i++) 
		{				
			commandList->SetGraphicsRootDescriptorTable(1, modelDataGPUdescriptorHandle);
			modelDataGPUdescriptorHandle.Offset(gpuDescriptorHeap->GetDescriptorSize());
			scene[i]->Render(commandList);
		}
	}
}

void SceneRendererDirectLightingVoxelGIandAO::InitShadowMapping(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
// Create resources for shadow mapping
{
	UpdateShadowMappingBuffers(_d3dDevice, backBufferWidth, backBufferHeight);
	
	// Create root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	root_signature_shadow_mapping.Reset(2, 0);
	root_signature_shadow_mapping[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	root_signature_shadow_mapping[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	root_signature_shadow_mapping.Finalize(_d3dDevice, L"Shadow Mapping pass RS", rootSignatureFlags);

	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("ShadowMapping_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	pipeline_state_shadow_mapping.SetRootSignature(root_signature_shadow_mapping);
	pipeline_state_shadow_mapping.SetRasterizerState(rasterizer_state_shadow_mapping);
	pipeline_state_shadow_mapping.SetRenderTargetFormats(0, nullptr, p_depth_buffer_shadow_mapping->GetFormat());
	pipeline_state_shadow_mapping.SetBlendState(blend_state_default);
	pipeline_state_shadow_mapping.SetDepthStencilState(depth_state_read_write);
	pipeline_state_shadow_mapping.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	pipeline_state_shadow_mapping.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipeline_state_shadow_mapping.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	pipeline_state_shadow_mapping.Finalize(_d3dDevice);

	delete[] pVertexShaderByteCode;
}
void SceneRendererDirectLightingVoxelGIandAO::RenderShadowMapping(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap, 
	std::vector<Model*>& scene,
	DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock, 
	DX12DescriptorHandleBlock& directionalLightDataDescriptorHeapBlock)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	{
		CD3DX12_VIEWPORT shadowMapViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, p_depth_buffer_shadow_mapping->GetWidth(), p_depth_buffer_shadow_mapping->GetHeight());
		CD3DX12_RECT shadowRect = CD3DX12_RECT(0.0f, 0.0f, p_depth_buffer_shadow_mapping->GetWidth(), p_depth_buffer_shadow_mapping->GetHeight());
		commandList->RSSetViewports(1, &shadowMapViewport);
		commandList->RSSetScissorRects(1, &shadowRect);

		commandList->SetPipelineState(pipeline_state_shadow_mapping.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(root_signature_shadow_mapping.GetSignature());

		_deviceResources->ResourceBarriersBegin(barriers);
		p_depth_buffer_shadow_mapping->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		_deviceResources->ResourceBarriersEnd(barriers, commandList);

		commandList->OMSetRenderTargets(0, nullptr, FALSE, &p_depth_buffer_shadow_mapping->GetDSV());
		commandList->ClearDepthStencilView(p_depth_buffer_shadow_mapping->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		commandList->SetGraphicsRootDescriptorTable(0, directionalLightDataDescriptorHeapBlock.GetGPUHandle());
		CD3DX12_GPU_DESCRIPTOR_HANDLE modelDataGPUdescriptorHandle = modelDataDescriptorHandleBlock.GetGPUHandle();
		for (auto& model : scene)
		{
			commandList->SetGraphicsRootDescriptorTable(1, modelDataGPUdescriptorHandle);
			modelDataGPUdescriptorHandle.Offset(gpuDescriptorHeap->GetDescriptorSize());
			model->Render(commandList);
		}

		// Reset back
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);
	}
}

void SceneRendererDirectLightingVoxelGIandAO::InitVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* _d3dDevice,
	float backBufferWidth, 
	float backBufferHeight,
	ID3D12GraphicsCommandList* _commandList)
{
	// Voxelization
	{
		viewport_vct_voxelization = CD3DX12_VIEWPORT(0.0f, 0.0f, shader_structure_cpu_voxelization_data.voxel_grid_res, shader_structure_cpu_voxelization_data.voxel_grid_res);
		scissor_rect_vct_voxelization = CD3DX12_RECT(0.0f, 0.0f, shader_structure_cpu_voxelization_data.voxel_grid_res, shader_structure_cpu_voxelization_data.voxel_grid_res);
		D3D12_SAMPLER_DESC shadowSampler;
		shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		shadowSampler.MipLODBias = 0;
		shadowSampler.MaxAnisotropy = 16;
		shadowSampler.MinLOD = 0.0f;
		shadowSampler.MaxLOD = D3D12_FLOAT32_MAX;
		shadowSampler.BorderColor[0] = 1.0f;
		shadowSampler.BorderColor[1] = 1.0f;
		shadowSampler.BorderColor[2] = 1.0f;
		shadowSampler.BorderColor[3] = 1.0f;

		UpdateVoxelConeTracingBuffers(_d3dDevice, backBufferWidth, backBufferHeight);
		UpdateVoxelConeTracingVoxelizationBuffers(_d3dDevice);

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		root_signature_vct_voxelization.Reset(5, 1);
		root_signature_vct_voxelization.InitStaticSampler(0, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
		root_signature_vct_voxelization[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization.Finalize(_d3dDevice, L"VCT voxelization pass RS", rootSignatureFlags);

		char* pVertexShaderByteCode;
		int vertexShaderByteCodeLength;
		char* pGeometryShadeByteCode;
		int geometryShaderByteCodeLength;
		char* pPixelShaderByteCode;
		int pixelShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelization_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelization_GS.cso"), pGeometryShadeByteCode, geometryShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelization_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		pipeline_state_vct_voxelization.SetRootSignature(root_signature_vct_voxelization);
		pipeline_state_vct_voxelization.SetRasterizerState(rasterizer_state_no_cull_no_depth);
		pipeline_state_vct_voxelization.SetRenderTargetFormats(0, nullptr, DXGI_FORMAT_D32_FLOAT);
		pipeline_state_vct_voxelization.SetBlendState(blend_state_default);
		pipeline_state_vct_voxelization.SetDepthStencilState(depth_state_disabled);
		pipeline_state_vct_voxelization.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		pipeline_state_vct_voxelization.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		pipeline_state_vct_voxelization.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
		pipeline_state_vct_voxelization.SetGeometryShader(pGeometryShadeByteCode, geometryShaderByteCodeLength);
		pipeline_state_vct_voxelization.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
		pipeline_state_vct_voxelization.Finalize(_d3dDevice);

		delete[] pVertexShaderByteCode;
		delete[] pGeometryShadeByteCode;
		delete[] pPixelShaderByteCode;

		//create constant buffer for pass
		DX12Buffer::Description cbDesc;
		cbDesc.element_size = c_aligned_shader_structure_cpu_voxelization_data;
		cbDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.descriptor_type = DX12Buffer::DescriptorType::CBV;
		p_constant_buffer_voxelization = new DX12Buffer(&descriptor_heap_manager,
			cbDesc,
			_deviceResources->GetBackBufferCount(),
			L"VCT Voxelization Pass CB");
	}

	// Vct debug generate indirect draw data 
	{
		// Create the indirect command signature
		D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
		commandSignatureDesc.pArgumentDescs = argumentDescs;
		commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
		commandSignatureDesc.ByteStride = sizeof(IndirectCommandCPU);

		ThrowIfFailed(_d3dDevice->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&indirect_draw_command_signature)));
		indirect_draw_command_signature->SetName(L"indirect_draw_command_signature");

		// Create the counter_reset_buffer
		UINT zero = 0;
		DX12Buffer::Description counterResetBufferDesc {};
		counterResetBufferDesc.descriptor_type = DX12Buffer::DescriptorType::SRV;
		counterResetBufferDesc.element_size = sizeof(UINT);
		counterResetBufferDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
		counterResetBufferDesc.format = DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
		p_counter_reset_buffer = new DX12Buffer(&descriptor_heap_manager, 
			counterResetBufferDesc, 
			1, 
			L"CounterReset Buffer",
			&zero,
			sizeof(UINT),
			_commandList);

		// Create the append buffer
		// The buffer will be large enought to hold all the voxels for the highest resolution voxel grid
		UINT numberOfVoxelsInTheGrid = shader_structure_cpu_voxelization_data.voxel_grid_res * shader_structure_cpu_voxelization_data.voxel_grid_res * shader_structure_cpu_voxelization_data.voxel_grid_res;
		DX12Buffer::Description appendBufferDesc {};
		appendBufferDesc.counter_offset = ((sizeof(ShaderStructureCPUVoxelDebugData) * numberOfVoxelsInTheGrid) + (D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1);
		appendBufferDesc.descriptor_type |= DX12Buffer::DescriptorType::SRV;
		appendBufferDesc.descriptor_type |= DX12Buffer::DescriptorType::UAV;
		appendBufferDesc.descriptor_type |= DX12Buffer::DescriptorType::Structured;
		appendBufferDesc.element_size = sizeof(ShaderStructureCPUVoxelDebugData);
		appendBufferDesc.num_of_elements = numberOfVoxelsInTheGrid;
		appendBufferDesc.state = D3D12_RESOURCE_STATE_COPY_DEST; // Because the first thing we do is reset the append buffer's counter
		p_append_buffer_indirect_draw_voxel_debug_per_instance_data = new DX12Buffer(&descriptor_heap_manager, 
			appendBufferDesc, 
			1, 
			L"Append buffer indirect draw voxel instance data");

		// Voxel debug cube
		p_voxel_debug_cube = new Model(&descriptor_heap_manager,
			_commandList,
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
			DirectX::XMQuaternionIdentity());

		// Create the indirect command buffer
		IndirectCommandCPU indirectCommand;
		indirectCommand.draw_indexed_arguments.IndexCountPerInstance = p_voxel_debug_cube->Meshes()[0]->GetIndicesNum();
		indirectCommand.draw_indexed_arguments.StartIndexLocation = 0;
		indirectCommand.draw_indexed_arguments.StartInstanceLocation = 0;
		indirectCommand.draw_indexed_arguments.BaseVertexLocation = 0;

		DX12Buffer::Description indirectCommandBufferDesc {};
		indirectCommandBufferDesc.descriptor_type |= DX12Buffer::DescriptorType::UAV;
		indirectCommandBufferDesc.descriptor_type |= DX12Buffer::DescriptorType::SRV;
		indirectCommandBufferDesc.descriptor_type |= DX12Buffer::DescriptorType::Structured;
		indirectCommandBufferDesc.element_size = sizeof(IndirectCommandCPU);
		indirectCommandBufferDesc.num_of_elements = 1;
		indirectCommandBufferDesc.state = D3D12_RESOURCE_STATE_COPY_DEST;
		p_indirect_command_buffer_voxel_debug = new DX12Buffer(&descriptor_heap_manager,
			indirectCommandBufferDesc,
			1,
			L"Indirect command buffer",
			&indirectCommand,
			sizeof(IndirectCommandCPU),
			_commandList);

		// Create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		root_signature_vct_debug_generate_indirect_draw_data.Reset(3, 1);
		root_signature_vct_debug_generate_indirect_draw_data.InitStaticSampler(0, sampler_bilinear, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug_generate_indirect_draw_data[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug_generate_indirect_draw_data[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug_generate_indirect_draw_data[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug_generate_indirect_draw_data.Finalize(_d3dDevice, L"VCT voxelization debug pass RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingDebugGenerateIndirectDrawData_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		pipeline_state_vct_debug_generate_indirect_draw_data.SetRootSignature(root_signature_vct_debug_generate_indirect_draw_data);
		pipeline_state_vct_debug_generate_indirect_draw_data.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_debug_generate_indirect_draw_data.Finalize(_d3dDevice);

		delete[] pComputeShaderByteCode;
	}

	// Vct debug 
	{
		p_render_target_vct_debug = new DX12RenderTarget(_d3dDevice,
			&descriptor_heap_manager,
			backBufferWidth,
			backBufferHeight, 
			_deviceResources->GetBackBufferFormat(), 
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 
			L"RenderTarget vct debug");

		// Create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		root_signature_vct_debug.Reset(3, 0);
		root_signature_vct_debug[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_debug.Finalize(_d3dDevice, L"VCT voxelization pass RS", rootSignatureFlags);

		char* pVertexShaderByteCode;
		int vertexShaderByteCodeLength;
		char* pPixelShaderByteCode;
		int pixelShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingDebugIndirectDraw_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingDebugIndirectDraw_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		DXGI_FORMAT formats[1];
		formats[0] = _deviceResources->GetBackBufferFormat();
		pipeline_state_vct_debug.SetRootSignature(root_signature_vct_debug);
		pipeline_state_vct_debug.SetRasterizerState(rasterizer_state_default);
		pipeline_state_vct_debug.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
		pipeline_state_vct_debug.SetBlendState(blend_state_default);
		pipeline_state_vct_debug.SetDepthStencilState(depth_state_read_write);
		pipeline_state_vct_debug.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		pipeline_state_vct_debug.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		pipeline_state_vct_debug.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
		pipeline_state_vct_debug.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
		pipeline_state_vct_debug.Finalize(_d3dDevice);

		delete[] pVertexShaderByteCode;
		delete[] pPixelShaderByteCode;
	}

	// Anisotropic mip generation
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero.Reset(3, 0);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero.Finalize(_d3dDevice, L"VCT aniso mipmapping level zero RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingAnisotropicMipmapChainGenerationLevelZero_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.SetRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation_level_zero);
		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.Finalize(_d3dDevice);

		delete[] pComputeShaderByteCode;

		root_signature_vct_voxelization_anisotropic_mip_generation.Reset(3, 0);
		root_signature_vct_voxelization_anisotropic_mip_generation[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation.Finalize(_d3dDevice, L"VCT aniso mipmapping RS", rootSignatureFlags);

		LoadShaderByteCode(GetFilePath("VoxelConeTracingAnisotropicMipmapChainGeneration_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		pipeline_state_vct_voxelization_anisotropic_mip_generation.SetRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation);
		pipeline_state_vct_voxelization_anisotropic_mip_generation.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_voxelization_anisotropic_mip_generation.Finalize(_d3dDevice);

		delete[] pComputeShaderByteCode;

		DX12Buffer::Description cbDesc;
		cbDesc.element_size = c_aligned_shader_structure_cpu_vct_anisotropic_mip_generation_data;
		cbDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.descriptor_type = DX12Buffer::DescriptorType::CBV;
		// The zero'th CB is for the VoxelConeTracingAnisotropicMipmapChainGenerationLevelZero_CS
		// the rest are for the VoxelConeTracingAnisotropicMipmapChainGeneration_CS
		for (UINT i = 0; i < shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count + 1; i++)
		{
			P_constant_buffers_vct_anisotropic_mip_generation.push_back(new DX12Buffer(&descriptor_heap_manager,
					cbDesc,
				    _deviceResources->GetBackBufferCount(),
					L"VCT aniso mip mapping CB"));
		}
	}

	// Voxel cone tracing main 
	{
		DX12Buffer::Description cbDesc;
		cbDesc.element_size = c_aligned_shader_structure_cpu_vct_main_data;
		cbDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.descriptor_type = DX12Buffer::DescriptorType::CBV;

		p_constant_buffer_vct_main = new DX12Buffer(&descriptor_heap_manager,
			cbDesc, 
			_deviceResources->GetBackBufferCount(),
			L"VCT main CB");

		// Create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;


		root_signature_vct_main.Reset(4, 1);
		root_signature_vct_main.InitStaticSampler(0, sampler_bilinear, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main.Finalize(_d3dDevice, L"VCT main pass compute version RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracing_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);


		pipeline_state_vct_main.SetRootSignature(root_signature_vct_main);
		pipeline_state_vct_main.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_main.Finalize(_d3dDevice);

		delete[] pComputeShaderByteCode;
	}

	// Upsample and blur
	{
		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		root_signature_vct_main_upsample_and_blur.Reset(2, 1);
		root_signature_vct_main_upsample_and_blur.InitStaticSampler(0, sampler_bilinear, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main_upsample_and_blur[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main_upsample_and_blur[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_main_upsample_and_blur.Finalize(_d3dDevice, L"VCT Main RT Upsample & Blur pass RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("UpsampleBlur_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		pipeline_state_vct_main_upsample_and_blur.SetRootSignature(root_signature_vct_main_upsample_and_blur);
		pipeline_state_vct_main_upsample_and_blur.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_main_upsample_and_blur.Finalize(_d3dDevice);

		delete[] pComputeShaderByteCode;
	}
}

void SceneRendererDirectLightingVoxelGIandAO::RenderVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources,
	ID3D12Device* _d3dDevice,
	ID3D12GraphicsCommandList* commandList,
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	std::vector<Model*>& scene,
	DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock,
	RenderQueue aQueue)
{
	if (!shader_structure_cpu_illumination_flags_data.use_vct)
	{
		return;
	}

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	if (aQueue == GRAPHICS_QUEUE) 
	{
		// Voxelizer
		{
			commandList->RSSetViewports(1, &viewport_vct_voxelization);
			commandList->RSSetScissorRects(1, &scissor_rect_vct_voxelization);
			commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

			commandList->SetPipelineState(pipeline_state_vct_voxelization.GetPipelineStateObject());
			commandList->SetGraphicsRootSignature(root_signature_vct_voxelization.GetSignature());

			_deviceResources->ResourceBarriersBegin(barriers);
			p_render_target_vct_voxelization->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			p_depth_buffer_shadow_mapping->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			_deviceResources->ResourceBarriersEnd(barriers, commandList);
			
			DX12DescriptorHandleBlock vctVoxelizationRenderTargetDescriptorHandleBlock;
			vctVoxelizationRenderTargetDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			vctVoxelizationRenderTargetDescriptorHandleBlock.Add(p_render_target_vct_voxelization->GetUAV());
			
			commandList->ClearUnorderedAccessViewFloat(vctVoxelizationRenderTargetDescriptorHandleBlock.GetGPUHandle(), 
				p_render_target_vct_voxelization->GetUAV(),
				p_render_target_vct_voxelization->GetResource(),
				gs_clear_color_black, 
				0, 
				nullptr);

			DX12DescriptorHandleBlock vctVoxelizationDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			vctVoxelizationDescriptorHandleBlock.Add(p_constant_buffer_voxelization->GetCBV(shader_structure_cpu_voxelization_data_most_updated_index));

			commandList->SetGraphicsRootDescriptorTable(0, vctVoxelizationDescriptorHandleBlock.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, directionalLightDescriptorHandleBlock.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(3, shadowDepthDescriptorHandleBlock.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(4, vctVoxelizationRenderTargetDescriptorHandleBlock.GetGPUHandle());
			CD3DX12_GPU_DESCRIPTOR_HANDLE modelDataGPUDescriptorHandle = modelDataDescriptorHandleBlock.GetGPUHandle();
			for (auto& model : scene) 
			{
				commandList->SetGraphicsRootDescriptorTable(2, modelDataGPUDescriptorHandle);
				modelDataGPUDescriptorHandle.Offset(gpuDescriptorHeap->GetDescriptorSize());
				model->Render(commandList);
			}

			// Reset back
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &rect);
		}
	}

	if (aQueue == COMPUTE_QUEUE) 
	{
		// Anisotropic mip chain generation
		{
			commandList->SetPipelineState(pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.GetPipelineStateObject());
			commandList->SetComputeRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation_level_zero.GetSignature());

			_deviceResources->ResourceBarriersBegin(barriers);
			p_render_target_vct_voxelization->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_deviceResources->ResourceBarriersEnd(barriers, commandList);

			DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			srvHandle.Add(p_render_target_vct_voxelization->GetSRV());

			DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(6);
			uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetUAV());
			uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetUAV());
			uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetUAV());
			uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetUAV());
			uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetUAV());
			uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetUAV());

			shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index++;
			if (shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index = 0;
			}

			shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension = shader_structure_cpu_voxelization_data.voxel_grid_res >> 1;
			shader_structure_cpu_vct_anisotropic_mip_generation_data.source_mip_level = 0;
			shader_structure_cpu_vct_anisotropic_mip_generation_data.result_mip_level = 0;
			memcpy(P_constant_buffers_vct_anisotropic_mip_generation[0]->GetMappedData(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index), &shader_structure_cpu_vct_anisotropic_mip_generation_data, sizeof(ShaderStructureCPUAnisotropicMipGenerationData));

			DX12DescriptorHandleBlock mipMappingDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			mipMappingDataDescriptorHandleBlock.Add(P_constant_buffers_vct_anisotropic_mip_generation[0]->GetCBV(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index));

			commandList->SetComputeRootDescriptorTable(0, mipMappingDataDescriptorHandleBlock.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());

			UINT dispatchBlockSize = ((shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension + (DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) & ~(DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) / DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION;
			commandList->Dispatch(dispatchBlockSize, dispatchBlockSize, dispatchBlockSize);

			// Dispatch the rest of the mips
			commandList->SetPipelineState(pipeline_state_vct_voxelization_anisotropic_mip_generation.GetPipelineStateObject());
			commandList->SetComputeRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation.GetSignature());
			for (UINT i = 1; i < shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count; i++)
			{
				// Move onto the next mip
				_deviceResources->ResourceBarriersBegin(barriers);
				P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				_deviceResources->ResourceBarriersEnd(barriers, commandList);

				DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(6);
				srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetSRV(i - 1));
				srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetSRV(i - 1));
				srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetSRV(i - 1));
				srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetSRV(i - 1));
				srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetSRV(i - 1));
				srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetSRV(i - 1));

				DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(6);
				uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetUAV(i));
				uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetUAV(i));
				uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetUAV(i));
				uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetUAV(i));
				uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetUAV(i));
				uavHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetUAV(i));

				shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension = shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension >> 1;
				shader_structure_cpu_vct_anisotropic_mip_generation_data.source_mip_level = i - 1;
				shader_structure_cpu_vct_anisotropic_mip_generation_data.result_mip_level = i;
				memcpy(P_constant_buffers_vct_anisotropic_mip_generation[i]->GetMappedData(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index), &shader_structure_cpu_vct_anisotropic_mip_generation_data, sizeof(ShaderStructureCPUAnisotropicMipGenerationData));

				DX12DescriptorHandleBlock mipMappingDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
				mipMappingDataDescriptorHandleBlock.Add(P_constant_buffers_vct_anisotropic_mip_generation[i]->GetCBV(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index));

				commandList->SetComputeRootDescriptorTable(0, mipMappingDataDescriptorHandleBlock.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());

				dispatchBlockSize = ((shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension + (DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) & ~(DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) / DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION;
				commandList->Dispatch(dispatchBlockSize, dispatchBlockSize, dispatchBlockSize);
			}

			// Transition the last mip level into a read only state as well
			_deviceResources->ResourceBarriersBegin(barriers);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			_deviceResources->ResourceBarriersEnd(barriers, commandList);
		}
		
		// Voxel cone tracing main
		{
			commandList->SetPipelineState(pipeline_state_vct_main.GetPipelineStateObject());
			commandList->SetComputeRootSignature(root_signature_vct_main.GetSignature());

			DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
			uavHandle.Add(p_render_target_vct_main->GetUAV());

			DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(10);
			srvHandle.Add(P_render_targets_gbuffer[0]->GetSRV());
			srvHandle.Add(P_render_targets_gbuffer[1]->GetSRV());
			srvHandle.Add(P_render_targets_gbuffer[2]->GetSRV());

			srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetSRV());
			srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetSRV());
			srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetSRV());
			srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetSRV());
			srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetSRV());
			srvHandle.Add(P_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetSRV());
			srvHandle.Add(p_render_target_vct_voxelization->GetSRV());

			DX12DescriptorHandleBlock cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			cbvHandle.Add(p_constant_buffer_voxelization->GetCBV(shader_structure_cpu_voxelization_data_most_updated_index));
			cbvHandle.Add(p_constant_buffer_vct_main->GetCBV(shader_structure_cpu_vct_main_data_most_updated_index));

			commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(1, cameraDataDescriptorHandleBlock.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, srvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(3, uavHandle.GetGPUHandle());

			commandList->Dispatch(DivideByMultiple(static_cast<UINT>(p_render_target_vct_main->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(p_render_target_vct_main->GetHeight()), 8u), 1u);
		}
	
		// Upsample and blur
		if (use_vct_main_upsample_and_blur == true)
		{
			{
				commandList->SetPipelineState(pipeline_state_vct_main_upsample_and_blur.GetPipelineStateObject());
				commandList->SetComputeRootSignature(root_signature_vct_main_upsample_and_blur.GetSignature());

				_deviceResources->ResourceBarriersBegin(barriers);
				p_render_target_vct_main->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				p_render_target_vct_main_upsample_and_blur->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				_deviceResources->ResourceBarriersEnd(barriers, commandList);

				DX12DescriptorHandleBlock srvHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				srvHandleBlur.Add(p_render_target_vct_main->GetSRV());

				DX12DescriptorHandleBlock uavHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				uavHandleBlur.Add(p_render_target_vct_main_upsample_and_blur->GetUAV());

				commandList->SetComputeRootDescriptorTable(0, srvHandleBlur.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, uavHandleBlur.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(p_render_target_vct_main_upsample_and_blur->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(p_render_target_vct_main_upsample_and_blur->GetHeight()), 8u), 1u);
			}
		}
	}
}

void SceneRendererDirectLightingVoxelGIandAO::RenderVoxelConeTracingDebug(DX12DeviceResourcesSingleton* _deviceResources,
	ID3D12Device* _d3dDevice,
	ID3D12GraphicsCommandList* _commandList,
	DX12DescriptorHeapGPU* _gpuDescriptorHeap,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
	RenderQueue aQueue)
{
	if (aQueue == COMPUTE_QUEUE)
	{
		// Vct debug generate indirect draw data 
		_commandList->SetPipelineState(pipeline_state_vct_debug_generate_indirect_draw_data.GetPipelineStateObject());
		_commandList->SetComputeRootSignature(root_signature_vct_debug_generate_indirect_draw_data.GetSignature());

		_deviceResources->ResourceBarriersBegin(barriers);
		p_render_target_vct_voxelization->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		p_append_buffer_indirect_draw_voxel_debug_per_instance_data->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_COPY_DEST);
		p_indirect_command_buffer_voxel_debug->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_COPY_DEST);
		_deviceResources->ResourceBarriersEnd(barriers, _commandList);

		_commandList->CopyBufferRegion(p_append_buffer_indirect_draw_voxel_debug_per_instance_data->GetResource(), 
			p_append_buffer_indirect_draw_voxel_debug_per_instance_data->GetDescription().counter_offset, 
			p_counter_reset_buffer->GetResource(), 0, sizeof(UINT));
		_commandList->CopyBufferRegion(p_indirect_command_buffer_voxel_debug->GetResource(),
			sizeof(UINT),
			p_counter_reset_buffer->GetResource(), 0, sizeof(UINT));

		_deviceResources->ResourceBarriersBegin(barriers);
		p_append_buffer_indirect_draw_voxel_debug_per_instance_data->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		p_indirect_command_buffer_voxel_debug->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		_deviceResources->ResourceBarriersEnd(barriers, _commandList);

		DX12DescriptorHandleBlock cbvHandle = _gpuDescriptorHeap->GetHandleBlock(1);
		cbvHandle.Add(p_constant_buffer_voxelization->GetCBV(shader_structure_cpu_voxelization_data_most_updated_index));

		DX12DescriptorHandleBlock srvHandle = _gpuDescriptorHeap->GetHandleBlock(1);
		srvHandle.Add(p_render_target_vct_voxelization->GetSRV());

		DX12DescriptorHandleBlock uavHandle = _gpuDescriptorHeap->GetHandleBlock(2);
		uavHandle.Add(p_append_buffer_indirect_draw_voxel_debug_per_instance_data->GetUAV());
		uavHandle.Add(p_indirect_command_buffer_voxel_debug->GetUAV());

		_commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
		_commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
		_commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());
		UINT dispatchBlockSize = Align(shader_structure_cpu_voxelization_data.voxel_grid_res, DISPATCH_BLOCK_SIZE_VCT_DEBUG_GENERATE_INDIRECT_DRAW_DATA) / DISPATCH_BLOCK_SIZE_VCT_DEBUG_GENERATE_INDIRECT_DRAW_DATA;
		_commandList->Dispatch(dispatchBlockSize, dispatchBlockSize, dispatchBlockSize);
	}

	if (aQueue == GRAPHICS_QUEUE)
	{
		_commandList->SetPipelineState(pipeline_state_vct_debug.GetPipelineStateObject());
		_commandList->SetComputeRootSignature(root_signature_vct_debug.GetSignature());
		_commandList->RSSetViewports(1, &_deviceResources->GetScreenViewport());
		_commandList->RSSetScissorRects(1, &_deviceResources->GetScissorRect());


		_deviceResources->ResourceBarriersBegin(barriers);
		p_indirect_command_buffer_voxel_debug->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		p_append_buffer_indirect_draw_voxel_debug_per_instance_data->TransitionTo(barriers, _commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_deviceResources->ResourceBarriersEnd(barriers, _commandList);

		DX12DescriptorHandleBlock cbvHandle = _gpuDescriptorHeap->GetHandleBlock(1);
		cbvHandle.Add(p_constant_buffer_voxelization->GetCBV(shader_structure_cpu_voxelization_data_most_updated_index));

		DX12DescriptorHandleBlock srvHandle = _gpuDescriptorHeap->GetHandleBlock(1);
		srvHandle.Add(p_append_buffer_indirect_draw_voxel_debug_per_instance_data->GetSRV());


		_commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
		_commandList->SetComputeRootDescriptorTable(1, cameraDataDescriptorHandleBlock.GetGPUHandle());
		_commandList->SetComputeRootDescriptorTable(2, srvHandle.GetGPUHandle());

		_commandList->ExecuteIndirect(
			indirect_draw_command_signature.Get(),
			1,
			p_indirect_command_buffer_voxel_debug->GetResource(),
			0,
			nullptr,
			0);
	}	
}

void SceneRendererDirectLightingVoxelGIandAO::InitLighting(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight)
{
	UpdateLightingBuffers(_d3dDevice, backBufferWidth, backBufferHeight);

	//create root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_SAMPLER_DESC shadowSampler;
	shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	shadowSampler.MipLODBias = 0;
	shadowSampler.MaxAnisotropy = 16;
	shadowSampler.MinLOD = 0.0f;
	shadowSampler.MaxLOD = D3D12_FLOAT32_MAX;
	shadowSampler.BorderColor[0] = 1.0f;
	shadowSampler.BorderColor[1] = 1.0f;
	shadowSampler.BorderColor[2] = 1.0f;
	shadowSampler.BorderColor[3] = 1.0f;

	root_signature_lighting.Reset(5, 2);
	root_signature_lighting.InitStaticSampler(0, sampler_bilinear, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_lighting.InitStaticSampler(1, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_lighting[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_lighting[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_lighting[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 3, 1, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_lighting[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_lighting[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_lighting.Finalize(_d3dDevice, L"Lighting pass RS", rootSignatureFlags);

	//PSO
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("Lighting_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode(GetFilePath("Lighting_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

	DXGI_FORMAT m_rtFormats[1];
	m_rtFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipeline_state_lighting.SetRootSignature(root_signature_lighting);
	pipeline_state_lighting.SetRasterizerState(rasterizer_state_default);
	pipeline_state_lighting.SetBlendState(blend_state_default);
	pipeline_state_lighting.SetDepthStencilState(depth_state_disabled);
	pipeline_state_lighting.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	pipeline_state_lighting.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipeline_state_lighting.SetRenderTargetFormats(_countof(m_rtFormats), m_rtFormats, DXGI_FORMAT_D32_FLOAT);
	pipeline_state_lighting.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	pipeline_state_lighting.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	pipeline_state_lighting.Finalize(_d3dDevice);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;

	//CB
	DX12Buffer::Description cbDesc;
	cbDesc.element_size = c_aligned_shader_structure_cpu_lighting_data;
	cbDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDesc.descriptor_type = DX12Buffer::DescriptorType::CBV;

	p_constant_buffer_lighting = new DX12Buffer(&descriptor_heap_manager,
		cbDesc,
		_deviceResources->GetBackBufferCount(),
		L"Lighting Pass CB");

	cbDesc.element_size = c_aligned_shader_structure_cpu_illumination_flags_data;
	p_constant_buffer_illumination_flags = new DX12Buffer(&descriptor_heap_manager,
		cbDesc, 
		_deviceResources->GetBackBufferCount(),
		L"Illumination Flags CB");
}
void SceneRendererDirectLightingVoxelGIandAO::RenderLighting(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock,
	D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &rect);

	commandList->SetPipelineState(pipeline_state_lighting.GetPipelineStateObject());
	commandList->SetGraphicsRootSignature(root_signature_lighting.GetSignature());

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLighting[] =
	{
		p_render_target_lighting->GetRTV()
	};

	_deviceResources->ResourceBarriersBegin(barriers);
	p_render_target_lighting->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	p_render_target_vct_main_upsample_and_blur->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	p_render_target_vct_main->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	P_render_targets_gbuffer[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	P_render_targets_gbuffer[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	P_render_targets_gbuffer[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	p_depth_buffer_shadow_mapping->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_deviceResources->ResourceBarriersEnd(barriers, commandList);

	commandList->OMSetRenderTargets(_countof(rtvHandlesLighting), rtvHandlesLighting, FALSE, nullptr);
	commandList->ClearRenderTargetView(rtvHandlesLighting[0], gs_clear_color_black, 0, nullptr);

	DX12DescriptorHandleBlock cbvHandleLighting = gpuDescriptorHeap->GetHandleBlock(2);
	cbvHandleLighting.Add(p_constant_buffer_lighting->GetCBV(shader_structure_cpu_lighting_data_most_updated_index));
	cbvHandleLighting.Add(p_constant_buffer_illumination_flags->GetCBV(shader_structure_cpu_illumination_flags_most_updated_index));

	DX12DescriptorHandleBlock srvHandleLighting = gpuDescriptorHeap->GetHandleBlock(4);
	srvHandleLighting.Add(P_render_targets_gbuffer[0]->GetSRV());
	srvHandleLighting.Add(P_render_targets_gbuffer[1]->GetSRV());
	srvHandleLighting.Add(P_render_targets_gbuffer[2]->GetSRV());

		
	if(use_vct_main_upsample_and_blur == true)
	{
		srvHandleLighting.Add(p_render_target_vct_main_upsample_and_blur->GetSRV());
	}
	else
	{
		srvHandleLighting.Add(p_render_target_vct_main->GetSRV());
	}
			
	commandList->SetGraphicsRootDescriptorTable(0, cbvHandleLighting.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(1, directionalLightDescriptorHandleBlock.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(2, cameraDataDescriptorHandleBlock.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(3, srvHandleLighting.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(4, shadowDepthDescriptorHandleBlock.GetGPUHandle());

	commandList->IASetVertexBuffers(0, 1, &fullScreenQuadVertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->DrawInstanced(4, 1, 0, 0);
}

void SceneRendererDirectLightingVoxelGIandAO::InitComposite(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* device)
{
	//create root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	root_signature_composite.Reset(1, 0);
	root_signature_composite[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_composite.Finalize(device, L"Composite RS", rootSignatureFlags);


	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("Composite_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode(GetFilePath("Composite_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);



	pipeline_state_composite = pipeline_state_lighting;
	pipeline_state_composite.SetRootSignature(root_signature_composite);
	pipeline_state_composite.SetRenderTargetFormat(_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_D32_FLOAT);
	pipeline_state_composite.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	pipeline_state_composite.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	pipeline_state_composite.Finalize(device);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
}
void SceneRendererDirectLightingVoxelGIandAO::RenderComposite(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView)
{
	
	commandList->SetPipelineState(pipeline_state_composite.GetPipelineStateObject());
	commandList->SetGraphicsRootSignature(root_signature_composite.GetSignature());

	_deviceResources->ResourceBarriersBegin(barriers);
	p_render_target_lighting->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_deviceResources->ResourceBarriersEnd(barriers, commandList);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
	{
			_deviceResources->GetRenderTargetView()
	};
	commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);
	commandList->ClearRenderTargetView(_deviceResources->GetRenderTargetView(), Colors::CornflowerBlue, 0, nullptr);
		
	DX12DescriptorHandleBlock srvHandleComposite = gpuDescriptorHeap->GetHandleBlock(1);
	srvHandleComposite.Add(p_render_target_lighting->GetSRV());
		
	commandList->SetGraphicsRootDescriptorTable(0, srvHandleComposite.GetGPUHandle());
	commandList->IASetVertexBuffers(0, 1, &fullScreenQuadVertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->DrawInstanced(4, 1, 0, 0);
	
}