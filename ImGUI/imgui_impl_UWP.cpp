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

#include "imgui.h"
#include "imgui_impl_UWP.h"
//#ifndef UWP_LEAN_AND_MEAN
//#define UWP_LEAN_AND_MEAN
//#endif
#include <windows.h>
//#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
//#include <tchar.h>
//#include <dwmapi.h>


// Configuration flags to add in your imconfig.h file:
//#define IMGUI_IMPL_UWP_DISABLE_GAMEPAD              // Disable gamepad support. This was meaningful before <1.81 but we now load XInput dynamically so the option is now less relevant.

// Using XInput for gamepad (will load DLL dynamically)
#ifndef IMGUI_IMPL_UWP_DISABLE_GAMEPAD
#define UWP_GAMEPAD_TRIGGER_THRESHOLD 0.3
#define UWP_GAMEPAD_THUMBSTICK_DEADZONE 0.2
/*
#include <xinput.h>
typedef DWORD (WINAPI *PFN_XInputGetCapabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD, XINPUT_STATE*);
*/
#endif

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2022-01-26: Inputs: replaced short-lived io.AddKeyModsEvent() (added two weeks ago)with io.AddKeyEvent() using ImGuiKey_ModXXX flags. Sorry for the confusion.
//  2021-01-20: Inputs: calling new io.AddKeyAnalogEvent() for gamepad support, instead of writing directly to io.NavInputs[].
//  2022-01-17: Inputs: calling new io.AddMousePosEvent(), io.AddMouseButtonEvent(), io.AddMouseWheelEvent() API (1.87+).
//  2022-01-17: Inputs: always update key mods next and before a key event (not in NewFrame) to fix input queue with very low framerates.
//  2022-01-12: Inputs: Update mouse inputs using WM_MOUSEMOVE/WM_MOUSELEAVE + fallback to provide it when focused but not hovered/captured. More standard and will allow us to pass it to future input queue API.
//  2022-01-12: Inputs: Maintain our own copy of MouseButtonsDown mask instead of using ImGui::IsAnyMouseDown() which will be obsoleted.
//  2022-01-10: Inputs: calling new io.AddKeyEvent(), io.AddKeyModsEvent() + io.SetKeyEventNativeData() API (1.87+). Support for full ImGuiKey range.
//  2021-12-16: Inputs: Fill VK_LCONTROL/VK_RCONTROL/VK_LSHIFT/VK_RSHIFT/VK_LMENU/VK_RMENU for completeness.
//  2021-08-17: Calling io.AddFocusEvent() on WM_SETFOCUS/WM_KILLFOCUS messages.
//  2021-08-02: Inputs: Fixed keyboard modifiers being reported when host window doesn't have focus.
//  2021-07-29: Inputs: MousePos is correctly reported when the host platform window is hovered but not focused (using TrackMouseEvent() to receive WM_MOUSELEAVE events).
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-06-08: Fixed ImGui_ImplUWP_EnableDpiAwareness() and ImGui_ImplUWP_GetDpiScaleForMonitor() to handle Windows 8.1/10 features without a manifest (per-monitor DPI, and properly calls SetProcessDpiAwareness() on 8.1).
//  2021-03-23: Inputs: Clearing keyboard down array when losing focus (WM_KILLFOCUS).
//  2021-02-18: Added ImGui_ImplUWP_EnableAlphaCompositing(). Non Visual Studio users will need to link with dwmapi.lib (MinGW/gcc: use -ldwmapi).
//  2021-02-17: Fixed ImGui_ImplUWP_EnableDpiAwareness() attempting to get SetProcessDpiAwareness from shcore.dll on Windows 8 whereas it is only supported on Windows 8.1.
//  2021-01-25: Inputs: Dynamically loading XInput DLL.
//  2020-12-04: Misc: Fixed setting of io.DisplaySize to invalid/uninitialized data when after hwnd has been closed.
//  2020-03-03: Inputs: Calling AddInputCharacterUTF16() to support surrogate pairs leading to codepoint >= 0x10000 (for more complete CJK inputs)
//  2020-02-17: Added ImGui_ImplUWP_EnableDpiAwareness(), ImGui_ImplUWP_GetDpiScaleForHwnd(), ImGui_ImplUWP_GetDpiScaleForMonitor() helper functions.
//  2020-01-14: Inputs: Added support for #define IMGUI_IMPL_UWP_DISABLE_GAMEPAD/IMGUI_IMPL_UWP_DISABLE_LINKING_XINPUT.
//  2019-12-05: Inputs: Added support for ImGuiMouseCursor_NotAllowed mouse cursor.
//  2019-05-11: Inputs: Don't filter value from WM_CHAR before calling AddInputCharacter().
//  2019-01-17: Misc: Using GetForegroundWindow()+IsChild() instead of GetActiveWindow() to be compatible with windows created in a different thread or parent.
//  2019-01-17: Inputs: Added support for mouse buttons 4 and 5 via WM_XBUTTON* messages.
//  2019-01-15: Inputs: Added support for XInput gamepads (if ImGuiConfigFlags_NavEnableGamepad is set by user application).
//  2018-11-30: Misc: Setting up io.BackendPlatformName so it can be displayed in the About Window.
//  2018-06-29: Inputs: Added support for the ImGuiMouseCursor_Hand cursor.
//  2018-06-10: Inputs: Fixed handling of mouse wheel messages to support fine position messages (typically sent by track-pads).
//  2018-06-08: Misc: Extracted imgui_impl_UWP.cpp/.h away from the old combined DX9/DX10/DX11/DX12 examples.
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors and ImGuiBackendFlags_HasSetMousePos flags + honor ImGuiConfigFlags_NoMouseCursorChange flag.
//  2018-02-20: Inputs: Added support for mouse cursors (ImGui::GetMouseCursor() value and WM_SETCURSOR message handling).
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-02-06: Inputs: Honoring the io.WantSetMousePos by repositioning the mouse (when using navigation and ImGuiConfigFlags_NavMoveMouse is set).
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-08: Inputs: Added mapping for ImGuiKey_Insert.
//  2018-01-05: Inputs: Added WM_LBUTTONDBLCLK double-click handlers for window classes with the CS_DBLCLKS flag.
//  2017-10-23: Inputs: Added WM_SYSKEYDOWN / WM_SYSKEYUP handlers so e.g. the VK_MENU key can be read.
//  2017-10-23: Inputs: Using UWP ::SetCapture/::GetCapture() to retrieve mouse positions outside the client area when dragging.
//  2016-11-12: Inputs: Only call UWP ::SetCursor(NULL) when io.MouseDrawCursor is set.

struct ImGui_ImplUWP_Data
{
    //HWND                              hWnd;
    HWND                                MouseHwnd;
    bool                                MouseTracked;
    //int                               MouseButtonsDown;
    INT64                               Time;
    INT64                               TicksPerSecond;
    ImGuiMouseCursor                    LastMouseCursor;
    Windows::Gaming::Input::Gamepad^    gamepad;
    //bool                              HasGamepad;
    //bool                              WantUpdateHasGamepad;

    /*
#ifndef IMGUI_IMPL_UWP_DISABLE_GAMEPAD
    HMODULE                     XInputDLL;
    PFN_XInputGetCapabilities   XInputGetCapabilities;
    PFN_XInputGetState          XInputGetState;
#endif
    */

    ImGui_ImplUWP_Data()      { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not well tested and probably dysfunctional in this backend.
// FIXME: some shared resources (mouse cursor shape, gamepad) are mishandled when using multi-context.
static ImGui_ImplUWP_Data* ImGui_ImplUWP_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplUWP_Data*)ImGui::GetIO().BackendPlatformUserData : NULL;
}

// Functions
bool    ImGui_ImplUWP_Init(/*void* hwnd*/)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == NULL && "Already initialized a platform backend!");

    INT64 perf_frequency, perf_counter;
    if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&perf_frequency))
        return false;
    if (!::QueryPerformanceCounter((LARGE_INTEGER*)&perf_counter))
        return false;

    // Setup backend capabilities flags
    ImGui_ImplUWP_Data* bd = IM_NEW(ImGui_ImplUWP_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_UWP";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    //bd->hWnd = (HWND)hwnd;
    //bd->WantUpdateHasGamepad = true;
    bd->TicksPerSecond = perf_frequency;
    bd->Time = perf_counter;
    bd->LastMouseCursor = ImGuiMouseCursor_COUNT;

    // Set platform dependent data in viewport
    //ImGui::GetMainViewport()->PlatformHandleRaw = (void*)hwnd;

    // Dynamically load XInput library
    
#ifndef IMGUI_IMPL_UWP_DISABLE_GAMEPAD
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    /*
    LPCWSTR* xinput_dll_names[] =
    {
        "xinput1_4.dll",   // Windows 8+
        "xinput1_3.dll",   // DirectX SDK
        "xinput9_1_0.dll", // Windows Vista, Windows 7
        "xinput1_2.dll",   // DirectX SDK
        "xinput1_1.dll"    // DirectX SDK
    };
    for (int n = 0; n < IM_ARRAYSIZE(xinput_dll_names); n++)
        if (HMODULE dll = ::LoadPackagedLibrary(xinput_dll_names[n]))
        {
            bd->XInputDLL = dll;
            bd->XInputGetCapabilities = (PFN_XInputGetCapabilities)::GetProcAddress(dll, "XInputGetCapabilities");
            bd->XInputGetState = (PFN_XInputGetState)::GetProcAddress(dll, "XInputGetState");
            break;
        }
    */
#endif // IMGUI_IMPL_UWP_DISABLE_GAMEPAD

    return true;
}

void    ImGui_ImplUWP_Shutdown()
{
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    IM_ASSERT(bd != NULL && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    // Unload XInput library
    /*
#ifndef IMGUI_IMPL_UWP_DISABLE_GAMEPAD
    if (bd->XInputDLL)
        ::FreeLibrary(bd->XInputDLL);
#endif // IMGUI_IMPL_UWP_DISABLE_GAMEPAD
*/

    io.BackendPlatformName = NULL;
    io.BackendPlatformUserData = NULL;
    IM_DELETE(bd);
}

static bool ImGui_ImplUWP_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    Windows::UI::Core::CoreWindow^ coreWindow = ImGui::GetCoreWindow();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        coreWindow->PointerCursor = nullptr;
        //::SetCursor(NULL);
    }
    else
    {
        // Show OS mouse cursor
        LPTSTR UWP_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
            case ImGuiMouseCursor_Arrow:        UWP_cursor = IDC_ARROW; 
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::Arrow, 0);
                break;
            }

            case ImGuiMouseCursor_TextInput:    UWP_cursor = IDC_IBEAM; 
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::IBeam, 0);

                break;
            }

            case ImGuiMouseCursor_ResizeAll:    UWP_cursor = IDC_SIZEALL;
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::SizeAll, 0);
                break;
            }

            case ImGuiMouseCursor_ResizeEW:     UWP_cursor = IDC_SIZEWE;
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::SizeWestEast, 0);
                break;
            }

            case ImGuiMouseCursor_ResizeNS:     UWP_cursor = IDC_SIZENS;
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::SizeNorthSouth, 0);
                break;
            }

            case ImGuiMouseCursor_ResizeNESW:   UWP_cursor = IDC_SIZENESW; 
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::SizeNortheastSouthwest, 0);
                break;
            }

            case ImGuiMouseCursor_ResizeNWSE:   UWP_cursor = IDC_SIZENWSE; 
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::SizeNorthwestSoutheast, 0);
                break;
            }

            case ImGuiMouseCursor_Hand:         UWP_cursor = IDC_HAND; 
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::Hand, 0);
                break;
            }

            case ImGuiMouseCursor_NotAllowed:   UWP_cursor = IDC_NO; 
            {
                coreWindow->PointerCursor = ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::UniversalNo, 0);
                break;
            }
        }
    }

    return true;
}

static bool IsVkDown(int vk)
{
    if ((ImGui::GetCoreWindow()->GetKeyState(static_cast<Windows::System::VirtualKey>(vk)) & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down)
    {
        return true;
    }

    return false;
    //return (::GetKeyState(vk) & 0x8000) != 0;
}


/*
static void ImGui_ImplUWP_AddKeyEvent(ImGuiKey key, bool down, int native_keycode, int native_scancode = -1)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, down);
    io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
    IM_UNUSED(native_scancode);
}
*/

static void ImGui_ImplUWP_ProcessKeyEventsWorkarounds()
{
    ImGuiIO& io = ImGui::GetIO();
    // Left & right Shift keys: when both are pressed together, Windows tend to not generate the WM_KEYUP event for the first released one.
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && !IsVkDown(VK_LSHIFT))
    {
        //ImGui_ImplUWP_AddKeyEvent(ImGuiKey_LeftShift, false, VK_LSHIFT);
        io.AddKeyEvent(ImGuiKey_LeftShift, false);
    }
        
    if (ImGui::IsKeyDown(ImGuiKey_RightShift) && !IsVkDown(VK_RSHIFT))
    {
        io.AddKeyEvent(ImGuiKey_RightShift, false);
        //ImGui_ImplUWP_AddKeyEvent(ImGuiKey_RightShift, false, VK_RSHIFT);
    }
       

    // Sometimes WM_KEYUP for Win key is not passed down to the app (e.g. for Win+V on some setups, according to GLFW).
    if (ImGui::IsKeyDown(ImGuiKey_LeftSuper) && !IsVkDown(VK_LWIN))
    {
        io.AddKeyEvent(ImGuiKey_LeftSuper, false);
        //ImGui_ImplUWP_AddKeyEvent(ImGuiKey_LeftSuper, false, VK_LWIN);
    }
        
    if (ImGui::IsKeyDown(ImGuiKey_RightSuper) && !IsVkDown(VK_RWIN))
    {
        io.AddKeyEvent(ImGuiKey_RightSuper, false);
        //ImGui_ImplUWP_AddKeyEvent(ImGuiKey_RightSuper, false, VK_RWIN);
    }
        
}

/*
static void ImGui_ImplUWP_UpdateKeyModifiers()
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_ModCtrl, IsVkDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiKey_ModShift, IsVkDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiKey_ModAlt, IsVkDown(VK_MENU));
    io.AddKeyEvent(ImGuiKey_ModSuper, IsVkDown(VK_APPS));
}
*/

static void ImGui_ImplUWP_UpdateMouseData()
{
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    //IM_ASSERT(bd->hWnd != 0);
    auto coreWindow = ImGui::GetCoreWindow();
    //const bool is_app_focused = (::GetForegroundWindow() == bd->hWnd);
    if (coreWindow->Visible)
    {
        // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
        if (io.WantSetMousePos)
        {
            coreWindow->PointerPosition = Windows::Foundation::Point(io.MousePos.x, io.MousePos.y);
            //POINT imGuiPos = { (int)io.MousePos.x, (int)io.MousePos.y };
            //if (::ClientToScreen(bd->hWnd, &pos))
                //::SetCursorPos(pos.x, pos.y);
        }

        // (Optional) Fallback to provide mouse position when focused (WM_MOUSEMOVE already provides this when hovered or captured)
        if (!io.WantSetMousePos && !bd->MouseTracked)
        {
            io.AddMousePosEvent(coreWindow->PointerPosition.X, coreWindow->PointerPosition.Y);
            //POINT pos;
            //if (::GetCursorPos(&pos) && ::ScreenToClient(bd->hWnd, &pos))
                //io.AddMousePosEvent((float)pos.x, (float)pos.y);
        }
    }
}

void ImGui_ImplUWP_GamepadConnectedDisconnected_Callback()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
    if (gamepads->Size == 0)
    {
        bd->gamepad = nullptr;
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    }
    else
    {
        bd->gamepad = gamepads->First()->Current;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    }
}

// Gamepad navigation mapping
static void ImGui_ImplUWP_UpdateGamepads()
{
#ifndef IMGUI_IMPL_UWP_DISABLE_GAMEPAD
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    if (bd->gamepad == nullptr)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    
    //if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0) // FIXME: Technically feeding gamepad shouldn't depend on this now that they are regular inputs.
    //    return;

    // Calling XInputGetState() every frame on disconnected gamepads is unfortunately too slow.
    // Instead we refresh gamepad availability by calling XInputGetCapabilities() _only_ after receiving WM_DEVICECHANGE.
    /*
    if (bd->WantUpdateHasGamepad)
    {
        XINPUT_CAPABILITIES caps = {};
        bd->HasGamepad = bd->XInputGetCapabilities ? (bd->XInputGetCapabilities(0, XINPUT_FLAG_GAMEPAD, &caps) == ERROR_SUCCESS) : false;
        bd->WantUpdateHasGamepad = false;
    }

    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    XINPUT_STATE xinput_state;
    XINPUT_GAMEPAD& gamepad = xinput_state.Gamepad;
    if (!bd->HasGamepad || bd->XInputGetState == NULL || bd->XInputGetState(0, &xinput_state) != ERROR_SUCCESS)
        return;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    */


    

    Windows::Gaming::Input::GamepadReading gamepadReading = bd->gamepad->GetCurrentReading();
    io.AddKeyEvent(ImGuiKey_GamepadStart, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::Menu));
    io.AddKeyEvent(ImGuiKey_GamepadBack, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::View));
    io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::X));
    io.AddKeyEvent(ImGuiKey_GamepadFaceRight, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::B));
    io.AddKeyEvent(ImGuiKey_GamepadFaceUp, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::Y));
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::A));
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::DPadLeft));
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::DPadRight));
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::DPadUp));
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::DPadDown));
    io.AddKeyEvent(ImGuiKey_GamepadL1, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::LeftShoulder));
    io.AddKeyEvent(ImGuiKey_GamepadR1, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::RightShoulder));
    io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, gamepadReading.LeftTrigger >= UWP_GAMEPAD_TRIGGER_THRESHOLD, gamepadReading.LeftTrigger);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, gamepadReading.RightTrigger >= UWP_GAMEPAD_TRIGGER_THRESHOLD, gamepadReading.RightTrigger);
    io.AddKeyEvent(ImGuiKey_GamepadL3, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::LeftThumbstick));
    io.AddKeyEvent(ImGuiKey_GamepadR3, static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::RightThumbstick));
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, gamepadReading.LeftThumbstickX <= -UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.LeftThumbstickX);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, gamepadReading.LeftThumbstickX >= UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.LeftThumbstickX);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, gamepadReading.LeftThumbstickY >= UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.LeftThumbstickY);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, gamepadReading.LeftThumbstickY <= -UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.LeftThumbstickY);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft, gamepadReading.RightThumbstickX <= -UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.RightThumbstickX);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, gamepadReading.RightThumbstickX >= UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.RightThumbstickX);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp, gamepadReading.RightThumbstickY >= UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.RightThumbstickY);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, gamepadReading.RightThumbstickY <= -UWP_GAMEPAD_THUMBSTICK_DEADZONE, gamepadReading.RightThumbstickY);
    

    /*
    #define IM_SATURATE(V)                      (V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
    #define MAP_BUTTON(KEY_NO, BUTTON_ENUM)     { io.AddKeyEvent(KEY_NO, (gamepad.wButtons & BUTTON_ENUM) != 0); }
    #define MAP_ANALOG(KEY_NO, VALUE, V0, V1)   { float vn = (float)(VALUE - V0) / (float)(V1 - V0); io.AddKeyAnalogEvent(KEY_NO, vn > 0.10f, IM_SATURATE(vn)); }
    MAP_BUTTON(ImGuiKey_GamepadStart,           XINPUT_GAMEPAD_START);
    MAP_BUTTON(ImGuiKey_GamepadBack,            XINPUT_GAMEPAD_BACK);
    MAP_BUTTON(ImGuiKey_GamepadFaceLeft,        XINPUT_GAMEPAD_X);
    MAP_BUTTON(ImGuiKey_GamepadFaceRight,       XINPUT_GAMEPAD_B);
    MAP_BUTTON(ImGuiKey_GamepadFaceUp,          XINPUT_GAMEPAD_Y);
    MAP_BUTTON(ImGuiKey_GamepadFaceDown,        XINPUT_GAMEPAD_A);
    MAP_BUTTON(ImGuiKey_GamepadDpadLeft,        XINPUT_GAMEPAD_DPAD_LEFT);
    MAP_BUTTON(ImGuiKey_GamepadDpadRight,       XINPUT_GAMEPAD_DPAD_RIGHT);
    MAP_BUTTON(ImGuiKey_GamepadDpadUp,          XINPUT_GAMEPAD_DPAD_UP);
    MAP_BUTTON(ImGuiKey_GamepadDpadDown,        XINPUT_GAMEPAD_DPAD_DOWN);
    MAP_BUTTON(ImGuiKey_GamepadL1,              XINPUT_GAMEPAD_LEFT_SHOULDER);
    MAP_BUTTON(ImGuiKey_GamepadR1,              XINPUT_GAMEPAD_RIGHT_SHOULDER);
    MAP_ANALOG(ImGuiKey_GamepadL2,              gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    MAP_ANALOG(ImGuiKey_GamepadR2,              gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    MAP_BUTTON(ImGuiKey_GamepadL3,              XINPUT_GAMEPAD_LEFT_THUMB);
    MAP_BUTTON(ImGuiKey_GamepadR3,              XINPUT_GAMEPAD_RIGHT_THUMB);
    MAP_ANALOG(ImGuiKey_GamepadLStickLeft,      gamepad.sThumbLX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickRight,     gamepad.sThumbLX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickUp,        gamepad.sThumbLY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickDown,      gamepad.sThumbLY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickLeft,      gamepad.sThumbRX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickRight,     gamepad.sThumbRX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickUp,        gamepad.sThumbRY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickDown,      gamepad.sThumbRY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    #undef MAP_BUTTON
    #undef MAP_ANALOG
    */
#endif // #ifndef IMGUI_IMPL_UWP_DISABLE_GAMEPAD
}

void    ImGui_ImplUWP_NewFrame()
{
    auto coreWindow = ImGui::GetCoreWindow();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplUWP_Init()?");

    // Setup display size (every frame to accommodate for window resizing)
    io.DisplaySize = ImVec2(coreWindow->Bounds.Width, coreWindow->Bounds.Height);
    //RECT rect = { 0, 0, 0, 0 };
    //::GetClientRect(bd->hWnd, &rect);
    //io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

    // Setup time step
    INT64 current_time = 0;
    ::QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    io.DeltaTime = (float)(current_time - bd->Time) / bd->TicksPerSecond;
    bd->Time = current_time;

    // Update OS mouse position
    ImGui_ImplUWP_UpdateMouseData();

    // Process workarounds for known Windows key handling issues
    ImGui_ImplUWP_ProcessKeyEventsWorkarounds();

    // Update OS mouse cursor with the cursor requested by imgui
    ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (bd->LastMouseCursor != mouse_cursor)
    {
        bd->LastMouseCursor = mouse_cursor;
        ImGui_ImplUWP_UpdateMouseCursor();
    }

    // Update game controllers (if enabled and available)
    ImGui_ImplUWP_UpdateGamepads();
}

// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

// Map VK_xxx to ImGuiKey_xxx.
static ImGuiKey ImGui_ImplUWP_VirtualKeyToImGuiKey(WPARAM wParam)
{
    switch (wParam)
    {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_PRIOR: return ImGuiKey_PageUp;
        case VK_NEXT: return ImGuiKey_PageDown;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_INSERT: return ImGuiKey_Insert;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_SPACE: return ImGuiKey_Space;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;
        case VK_OEM_7: return ImGuiKey_Apostrophe;
        case VK_OEM_COMMA: return ImGuiKey_Comma;
        case VK_OEM_MINUS: return ImGuiKey_Minus;
        case VK_OEM_PERIOD: return ImGuiKey_Period;
        case VK_OEM_2: return ImGuiKey_Slash;
        case VK_OEM_1: return ImGuiKey_Semicolon;
        case VK_OEM_PLUS: return ImGuiKey_Equal;
        case VK_OEM_4: return ImGuiKey_LeftBracket;
        case VK_OEM_5: return ImGuiKey_Backslash;
        case VK_OEM_6: return ImGuiKey_RightBracket;
        case VK_OEM_3: return ImGuiKey_GraveAccent;
        case VK_CAPITAL: return ImGuiKey_CapsLock;
        case VK_SCROLL: return ImGuiKey_ScrollLock;
        case VK_NUMLOCK: return ImGuiKey_NumLock;
        case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
        case VK_PAUSE: return ImGuiKey_Pause;
        case VK_NUMPAD0: return ImGuiKey_Keypad0;
        case VK_NUMPAD1: return ImGuiKey_Keypad1;
        case VK_NUMPAD2: return ImGuiKey_Keypad2;
        case VK_NUMPAD3: return ImGuiKey_Keypad3;
        case VK_NUMPAD4: return ImGuiKey_Keypad4;
        case VK_NUMPAD5: return ImGuiKey_Keypad5;
        case VK_NUMPAD6: return ImGuiKey_Keypad6;
        case VK_NUMPAD7: return ImGuiKey_Keypad7;
        case VK_NUMPAD8: return ImGuiKey_Keypad8;
        case VK_NUMPAD9: return ImGuiKey_Keypad9;
        case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE: return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case VK_ADD: return ImGuiKey_KeypadAdd;
        case IM_VK_KEYPAD_ENTER: return ImGuiKey_KeypadEnter;
        //////////////////////////////////////////////////////////////// Added mod struff, previously handled by ImGui_ImplUWP_UpdateKeyModifiers
        ////////////////////////////////////////////////////////////////
        case VK_SHIFT: return ImGuiKey_ModShift;
        case VK_CONTROL: return ImGuiKey_ModCtrl;
        case VK_MENU: return ImGuiKey_ModAlt;
        case VK_APPS: return ImGuiKey_ModSuper;
        ////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////
        case VK_LSHIFT: return ImGuiKey_LeftShift;
        case VK_LCONTROL: return ImGuiKey_LeftCtrl;
        case VK_LMENU: return ImGuiKey_LeftAlt;
        case VK_LWIN: return ImGuiKey_LeftSuper;
        case VK_RSHIFT: return ImGuiKey_RightShift;
        case VK_RCONTROL: return ImGuiKey_RightCtrl;
        case VK_RMENU: return ImGuiKey_RightAlt;
        case VK_RWIN: return ImGuiKey_RightSuper;
        //case VK_APPS: return ImGuiKey_Menu;
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;
        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;
        case VK_F1: return ImGuiKey_F1;
        case VK_F2: return ImGuiKey_F2;
        case VK_F3: return ImGuiKey_F3;
        case VK_F4: return ImGuiKey_F4;
        case VK_F5: return ImGuiKey_F5;
        case VK_F6: return ImGuiKey_F6;
        case VK_F7: return ImGuiKey_F7;
        case VK_F8: return ImGuiKey_F8;
        case VK_F9: return ImGuiKey_F9;
        case VK_F10: return ImGuiKey_F10;
        case VK_F11: return ImGuiKey_F11;
        case VK_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

// Allow compilation with old Windows SDK. MinGW doesn't have default _UWP_WINNT/WINVER versions.
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED 0x0007
#endif

// UWP message handler (process UWP mouse/keyboard inputs, etc.)
// Call from your application's message handler. Keep calling your message handler unless this function returns TRUE.
// When implementing your own backend, you can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if Dear ImGui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to Dear ImGui, and hide them from your application based on those two flags.
// PS: In this UWP handler, we use the capture API (GetCapture/SetCapture/ReleaseCapture) to be able to read mouse coordinates when dragging mouse outside of our window bounds.
// PS: We treat DBLCLK messages as regular mouse down messages, so this code will work on windows classes that have the CS_DBLCLKS flag set. Our own example app code doesn't set this flag.
#if 0
// Copy this line into your .cpp file to forward declare the function.
extern IMGUI_IMPL_API LRESULT ImGui_ImplUWP_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

void ImGui_ImplUWP_PointerMoved_Callback(float x, float y)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
}

void ImGui_ImplUWP_PointerEntered_Callback()
{
    //ImGui::GetCoreWindow()->SetPointerCapture();
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    bd->MouseTracked = true;
}

void ImGui_ImplUWP_PointerExited_Callback()
{
    //ImGui::GetCoreWindow()->ReleasePointerCapture();
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    bd->MouseTracked = false;
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

void ImGui_ImplUWP_PointerPressed_Callback(Windows::UI::Input::PointerPointProperties^ pointerPointProterties)
{
    int button = 0;
    if (pointerPointProterties->IsRightButtonPressed)
    {
        button = 1;
    }
    else
    {
        if (pointerPointProterties->IsMiddleButtonPressed)
        {
            button = 2;
        }
        else
        {
            if (pointerPointProterties->IsXButton1Pressed)
            {
                button = 3;
            }
            else
            {
                if (pointerPointProterties->IsXButton2Pressed)
                {
                    button = 4;
                }
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, true);
}

void ImGui_ImplUWP_PointerReleased_Callback(Windows::UI::Input::PointerPointProperties^ pointerPointProterties)
{
    int button = 0;
    if (pointerPointProterties->IsRightButtonPressed)
    {
        button = 1;
    }
    else
    {
        if (pointerPointProterties->IsMiddleButtonPressed)
        {
            button = 2;
        }
        else
        {
            if (pointerPointProterties->IsXButton1Pressed)
            {
                button = 3;
            }
            else
            {
                if (pointerPointProterties->IsXButton2Pressed)
                {
                    button = 4;
                }
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, false);
}

void ImGui_ImplUWP_PointerWheelChanged_Callback(int wheelDelta)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(0.0f, wheelDelta);
}

void ImGui_ImplUWP_KeyEvent_Callback(int vk, bool down)
{
    ImGuiIO& io = ImGui::GetIO();
    const ImGuiKey key = ImGui_ImplUWP_VirtualKeyToImGuiKey(vk);
    if (key != ImGuiKey_None)
    {
        io.AddKeyEvent(key, down);
    }  
}

void ImGui_ImplUWP_VisibilityChanged_Callback(bool isVisible)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(isVisible);
}


IMGUI_IMPL_API LRESULT ImGui_ImplUWP_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() == NULL)
        return 0;

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplUWP_Data* bd = ImGui_ImplUWP_GetBackendData();

    switch (msg)
    {
        /*
        case WM_MOUSEMOVE:
            // We need to call TrackMouseEvent in order to receive WM_MOUSELEAVE events
            bd->MouseHwnd = hwnd;
            if (!bd->MouseTracked)
            {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                ::TrackMouseEvent(&tme);
                bd->MouseTracked = true;
            }
            io.AddMousePosEvent((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
            break;
        case WM_MOUSELEAVE:
            if (bd->MouseHwnd == hwnd)
                bd->MouseHwnd = NULL;
            bd->MouseTracked = false;
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            break;
            
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        {
            int button = 0;
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
            if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
            if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
            if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
            if (bd->MouseButtonsDown == 0 && ::GetCapture() == NULL)
                ::SetCapture(hwnd);
            bd->MouseButtonsDown |= 1 << button;
            io.AddMouseButtonEvent(button, true);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            int button = 0;
            if (msg == WM_LBUTTONUP) { button = 0; }
            if (msg == WM_RBUTTONUP) { button = 1; }
            if (msg == WM_MBUTTONUP) { button = 2; }
            if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
            bd->MouseButtonsDown &= ~(1 << button);
            if (bd->MouseButtonsDown == 0 && ::GetCapture() == hwnd)
                ::ReleaseCapture();
            io.AddMouseButtonEvent(button, false);
            return 0;
        }
        
        case WM_MOUSEWHEEL:
            io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
            return 0;
        case WM_MOUSEHWHEEL:
            io.AddMouseWheelEvent((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
            return 0;
        
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            const bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            if (wParam < 256)
            {
                // Submit modifiers
                ImGui_ImplUWP_UpdateKeyModifiers();

                // Obtain virtual key code
                // (keypad enter doesn't have its own... VK_RETURN with KF_EXTENDED flag means keypad enter, see IM_VK_KEYPAD_ENTER definition for details, it is mapped to ImGuiKey_KeyPadEnter.)
                int vk = (int)wParam;
                if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
                    vk = IM_VK_KEYPAD_ENTER;

                // Submit key event
                const ImGuiKey key = ImGui_ImplUWP_VirtualKeyToImGuiKey(vk);
                const int scancode = (int)LOBYTE(HIWORD(lParam));
                if (key != ImGuiKey_None)
                    ImGui_ImplUWP_AddKeyEvent(key, is_key_down, vk, scancode);
                    

                // Submit individual left/right modifier events
                if (vk == VK_SHIFT)
                {
                    // Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplUWP_ProcessKeyEventsWorkarounds()
                    if (IsVkDown(VK_LSHIFT) == is_key_down) { ImGui_ImplUWP_AddKeyEvent(ImGuiKey_LeftShift, is_key_down, VK_LSHIFT, scancode); }
                    if (IsVkDown(VK_RSHIFT) == is_key_down) { ImGui_ImplUWP_AddKeyEvent(ImGuiKey_RightShift, is_key_down, VK_RSHIFT, scancode); }
                }
                else if (vk == VK_CONTROL)
                {
                    if (IsVkDown(VK_LCONTROL) == is_key_down) { ImGui_ImplUWP_AddKeyEvent(ImGuiKey_LeftCtrl, is_key_down, VK_LCONTROL, scancode); }
                    if (IsVkDown(VK_RCONTROL) == is_key_down) { ImGui_ImplUWP_AddKeyEvent(ImGuiKey_RightCtrl, is_key_down, VK_RCONTROL, scancode); }
                }
                else if (vk == VK_MENU)
                {
                    if (IsVkDown(VK_LMENU) == is_key_down) { ImGui_ImplUWP_AddKeyEvent(ImGuiKey_LeftAlt, is_key_down, VK_LMENU, scancode); }
                    if (IsVkDown(VK_RMENU) == is_key_down) { ImGui_ImplUWP_AddKeyEvent(ImGuiKey_RightAlt, is_key_down, VK_RMENU, scancode); }
                }
            }
            return 0;
        }
        
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            io.AddFocusEvent(msg == WM_SETFOCUS);
            return 0;
        
        case WM_CHAR:
            // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
            if (wParam > 0 && wParam < 0x10000)
                io.AddInputCharacterUTF16((unsigned short)wParam);
            return 0;
            
        case WM_SETCURSOR:
            // This is required to restore cursor when transitioning from e.g resize borders to client area.
            if (LOWORD(lParam) == HTCLIENT && ImGui_ImplUWP_UpdateMouseCursor())
                return 1;
            return 0;
            
        case WM_DEVICECHANGE:
            if ((UINT)wParam == DBT_DEVNODES_CHANGED)
                bd->WantUpdateHasGamepad = true;
            return 0;
            */
    }

    return 0;
}