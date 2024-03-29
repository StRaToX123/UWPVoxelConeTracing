// dear imgui: Platform Backend for Windows (standard windows API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)

// Implemented features:
//  [X] Platform: Clipboard support (for UWP this is actually part of core dear imgui)
//  [X] Platform: Keyboard support. Since 1.87 we are using the io.AddKeyEvent() function. Pass ImGuiKey values to all key functions e.g. ImGui::IsKeyPressed(ImGuiKey_Space). [Legacy VK_* values will also be supported unless IMGUI_DISABLE_OBSOLETE_KEYIO is set]
//  [X] Platform: Gamepad support. Enabled with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.
//  [X] Platform: Mouse cursor shape and visibility. Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#include "Utility\Debug\DebugMessage.h"

IMGUI_IMPL_API bool     ImGui_ImplUWP_Init(/*void* hwnd*/);
IMGUI_IMPL_API void     ImGui_ImplUWP_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplUWP_NewFrame();
// These callbacks need to be called in a platform sepecifici way.
// For a UWP app these are called via CoreWindow events that are setup in the App.cpp file
IMGUI_IMPL_API void     ImGui_ImplUWP_PointerMoved_Callback(float x, float y);
IMGUI_IMPL_API void     ImGui_ImplUWP_PointerEntered_Callback();
IMGUI_IMPL_API void     ImGui_ImplUWP_PointerExited_Callback();
IMGUI_IMPL_API void     ImGui_ImplUWP_PointerButton_Callback(Windows::UI::Input::PointerPointProperties^ pointerPointProterties);
IMGUI_IMPL_API void     ImGui_ImplUWP_PointerWheelChanged_Callback(int wheelDelta);
IMGUI_IMPL_API void     ImGui_ImplUWP_KeyEvent_Callback(int vk, bool down);
IMGUI_IMPL_API void     ImGui_ImplUWP_VisibilityChanged_Callback(bool isVisible);
IMGUI_IMPL_API void     ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(bool padAvailable);
IMGUI_IMPL_API void     ImGui_ImplUWP_UpdateGamepads_Callback(const Windows::Gaming::Input::GamepadReading& gamepadReading);


// UWP message handler your application need to call.
// - Intentionally commented out in a '#if 0' block to avoid dragging dependencies on <windows.h> from this helper.
// - You should COPY the line below into your .cpp code to forward declare the function and then you can call it.
// - Call from your application's message handler. Keep calling your message handler unless this function returns TRUE.

#if 0
extern IMGUI_IMPL_API LRESULT ImGui_ImplUWP_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
