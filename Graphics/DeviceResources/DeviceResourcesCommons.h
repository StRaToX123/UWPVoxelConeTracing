#pragma once


namespace DX
{
	// Use triple buffering.
	static const UINT gsc_frame_count = 2;		
	// Starts at 1 since the first call to the Swapchain's Present, presents the backbuffer at index 1.
	// So we wanna start start rendering at that backbuffer first.
	static UINT gs_current_back_buffer_index = 1;
}