// === win32_dx11.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifdef _WIN32

#  include "imgui.h"
#  include "imgui_impl_dx11.h"
#  include "imgui_impl_win32.h"

// sen
#  include "sen/core/base/assert.h"

// d3d
#  include <d3d11.h>

// std
#  include <tchar.h>

#  include <stdexcept>

//--------------------------------------------------------------------------------------------------------------
// Data
//--------------------------------------------------------------------------------------------------------------

static ID3D11Device* pd3dDevice = nullptr;
static ID3D11DeviceContext* pd3dDeviceContext = nullptr;
static IDXGISwapChain* pSwapChain = nullptr;
static ID3D11RenderTargetView* mainRenderTargetView = nullptr;
static HWND windowHandle = nullptr;
static WNDCLASSEX windowClass = {};

//--------------------------------------------------------------------------------------------------------------
// Forward declarations of helper functions
//--------------------------------------------------------------------------------------------------------------

bool createDeviceD3D(HWND hWnd);
void cleanupDeviceD3D();
void createRenderTarget();
void cleanupRenderTarget();
LRESULT WINAPI wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//--------------------------------------------------------------------------------------------------------------
// Backend implementation
//--------------------------------------------------------------------------------------------------------------

void backendOpen()
{
  // Create application window

  // ImGui_ImplWin32_EnableDpiAwareness();
  windowClass = {sizeof(WNDCLASSEX),
                 CS_CLASSDC,
                 wndProc,
                 0L,
                 0L,
                 GetModuleHandle(nullptr),
                 nullptr,
                 nullptr,
                 nullptr,
                 nullptr,
                 _T("Sen Explorer"),
                 nullptr};

  ::RegisterClassEx(&windowClass);

  windowHandle = ::CreateWindow(windowClass.lpszClassName,
                                _T("Sen Explorer"),
                                WS_OVERLAPPEDWINDOW,
                                100,
                                100,
                                1280,
                                800,
                                nullptr,
                                nullptr,
                                windowClass.hInstance,
                                nullptr);

  // Initialize Direct3D
  if (!createDeviceD3D(windowHandle))
  {
    cleanupDeviceD3D();
    ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
    ::sen::throwRuntimeError("could not create Direct3D device");
  }

  // Show the window
  ::ShowWindow(windowHandle, SW_SHOWMAXIMIZED);
  ::UpdateWindow(windowHandle);
}

void backendSetup()
{
  // Setup Platform/Renderer backends
  ImGui_ImplWin32_Init(windowHandle);
  ImGui_ImplDX11_Init(pd3dDevice, pd3dDeviceContext);
}

bool backendPreUpdate()
{
  // Poll and handle messages (inputs, window resize, etc.)
  // See the wndProc() function below for our to dispatch events to the Win32 backend.
  MSG msg;
  while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
  {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    if (msg.message == WM_QUIT)
    {
      return true;
    }
  }

  // Start the Dear ImGui frame
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();

  return false;  // not done yet
}

void backendPostUpdate(const ImVec4& clearColor)
{
  const float clearColorArray[4U] = {
    clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w};

  pd3dDeviceContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
  pd3dDeviceContext->ClearRenderTargetView(mainRenderTargetView, clearColorArray);

  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  pSwapChain->Present(1, 0);  // Present with vsync
}

void backendClose()
{
  // Cleanup
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();

  cleanupDeviceD3D();
  ::DestroyWindow(windowHandle);
  ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

//--------------------------------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------------------------------

bool createDeviceD3D(HWND hWnd)
{
  // Setup swap chain
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_0,
  };

  if (D3D11CreateDeviceAndSwapChain(nullptr,
                                    D3D_DRIVER_TYPE_HARDWARE,
                                    nullptr,
                                    createDeviceFlags,
                                    featureLevelArray,
                                    2,
                                    D3D11_SDK_VERSION,
                                    &sd,
                                    &pSwapChain,
                                    &pd3dDevice,
                                    &featureLevel,
                                    &pd3dDeviceContext) != S_OK)
  {
    return false;
  }

  createRenderTarget();
  return true;
}

void cleanupDeviceD3D()
{
  cleanupRenderTarget();
  if (pSwapChain)
  {
    pSwapChain->Release();
    pSwapChain = nullptr;
  }
  if (pd3dDeviceContext)
  {
    pd3dDeviceContext->Release();
    pd3dDeviceContext = nullptr;
  }
  if (pd3dDevice)
  {
    pd3dDevice->Release();
    pd3dDevice = nullptr;
  }
}

void createRenderTarget()
{
  ID3D11Texture2D* pBackBuffer;
  pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
  pBackBuffer->Release();
}

void cleanupRenderTarget()
{
  if (mainRenderTargetView)
  {
    mainRenderTargetView->Release();
    mainRenderTargetView = nullptr;
  }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite
// your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
// clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and hide
// them from your application based on those two flags.
LRESULT WINAPI wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
  {
    return true;
  }

  switch (msg)
  {
    case WM_SIZE:
    {
      if (pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
      {
        cleanupRenderTarget();
        pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
        createRenderTarget();
      }
      return 0;
    }

    case WM_SYSCOMMAND:
    {
      if ((wParam & 0xfff0) == SC_KEYMENU)  // Disable ALT application menu
      {
        return 0;
      }
      break;
    }

    case WM_DESTROY:
    {
      ::PostQuitMessage(0);
    }
      return 0;
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

#endif
