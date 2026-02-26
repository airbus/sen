// === sdl2.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// NOLINTBEGIN

// imgui
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

// sen
#include "sen/core/base/assert.h"

// std
#include <string>

// sdl
#include <SDL2/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#  include <SDL2/SDL_opengles2.h>
#else
#  include <SDL2/SDL_opengl.h>
#endif

#ifdef __linux__
#  include <stdlib.h>
#endif

//--------------------------------------------------------------------------------------------------------------
// Data
//--------------------------------------------------------------------------------------------------------------

static SDL_Window* window = nullptr;
static SDL_GLContext glContext = nullptr;
static const char* glslVersion = nullptr;

//--------------------------------------------------------------------------------------------------------------
// Backend implementation
//--------------------------------------------------------------------------------------------------------------

void backendOpen()
{
  // Make sure the window has a meaningful X11 class name
  // With SDL3 this should be replaced with #define SDL_HINT_APP_ID "sen-explorer" (probably set from CMake)
#ifdef __linux__
  if (setenv("SDL_VIDEO_X11_WMCLASS", "sen-explorer", 0))
  {
    sen::throwRuntimeError("Out of memory!");
  }
#endif
  // Setup SDL
  // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows
  // systems, depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL
  // is recommended!)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    sen::throwRuntimeError(std::string(SDL_GetError()));
  }

  // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  glslVersion = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  // GL 3.2 Core + GLSL 150
  glslVersion = "#version 150";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  glslVersion = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);   // NOLINT
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);  // NOLINT
  auto windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_OPENGL | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE |
                                                  SDL_WINDOW_ALLOW_HIGHDPI);

  // NOLINTNEXTLINE
  window = SDL_CreateWindow("Sen Explorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, windowFlags);
  if (window == nullptr)
  {
    sen::throwRuntimeError(std::string(SDL_GetError()));
  }

  glContext = SDL_GL_CreateContext(window);
  if (glContext == nullptr)
  {
    sen::throwRuntimeError(std::string(SDL_GetError()));
  }

  SDL_GL_MakeCurrent(window, glContext);
  SDL_GL_SetSwapInterval(1);  // Enable vsync
}

void backendSetup()
{
  ImGui_ImplSDL2_InitForOpenGL(window, glContext);
  ImGui_ImplOpenGL3_Init(glslVersion);
}

bool backendPreUpdate()
{
  bool done = false;

  SDL_Event event;
  while (SDL_PollEvent(&event) != 0)
  {
    ImGui_ImplSDL2_ProcessEvent(&event);
    if (event.type == SDL_QUIT)
    {
      done = true;
    }
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
        event.window.windowID == SDL_GetWindowID(window))
    {
      done = true;
    }
  }

  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  return done;
}

void backendPostUpdate(const ImVec4& clearColor)
{
  const auto& io = ImGui::GetIO();

  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);  // NOLINT
  glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);

  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window);
}

void backendClose()
{
  ImGui_ImplSDL2_Shutdown();
  ImGui_ImplOpenGL3_Shutdown();

  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

// NOLINTEND
