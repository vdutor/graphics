/* Copyright 2019 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow_graphics/rendering/opengl/egl_offscreen_context.h"

#include <EGL/egl.h>

#include "GL/util/egl_util.h"
#include "tensorflow_graphics/rendering/opengl/gl_macros.h"
#include "tensorflow_graphics/util/cleanup.h"

EGLOffscreenContext::EGLOffscreenContext(EGLContext context, EGLDisplay display,
                                         EGLSurface pixel_buffer_surface)
    : context_(context),
      display_(display),
      pixel_buffer_surface_(pixel_buffer_surface) {}

EGLOffscreenContext::~EGLOffscreenContext() {
  EGLBoolean success;

  success = Release();
  if (success == false)
    std::cerr << "EGLOffscreenContext::~EGLOffscreenContext: an error occured "
                 "in Release."
              << std::endl;
  success = eglDestroyContext(display_, context_);
  if (success == false)
    std::cerr << "EGLOffscreenContext::~EGLOffscreenContext: an error occured "
                 "in eglDestroyContext."
              << std::endl;
  success = eglDestroySurface(display_, pixel_buffer_surface_);
  if (success == false)
    std::cerr << "EGLOffscreenContext::~EGLOffscreenContext: an error occured "
                 "in eglDestroySurface."
              << std::endl;
  success = TerminateInitializedEGLDisplay(display_);
  if (success == false)
    std::cerr << "EGLOffscreenContext::~EGLOffscreenContext: an error occured "
                 "in TerminateInitializedEGLDisplay."
              << std::endl;
}

bool EGLOffscreenContext::Create(
    std::unique_ptr<EGLOffscreenContext>* egl_offscreen_context,
    const int pixel_buffer_width, const int pixel_buffer_height,
    const EGLenum rendering_api, const EGLint* configuration_attributes,
    const EGLint* context_attributes) {
  EGLBoolean success;
  EGLContext context;
  EGLDisplay display;
  EGLSurface pixel_buffer_surface;

  // Create an EGL display at device index 0.
  display = CreateInitializedEGLDisplay();
  if (display == EGL_NO_DISPLAY) return false;
  auto initialize_cleanup =
      MakeCleanup([display]() { TerminateInitializedEGLDisplay(display); });

  // Set the current rendering API.
  RETURN_FALSE_IF_EGL_ERROR(success = eglBindAPI(rendering_api));
  if (success == false) return false;

  // Build a frame buffer configuration.
  EGLConfig frame_buffer_configuration;
  EGLint returned_num_configs;
  const EGLint kRequestedNumConfigs = 1;

  RETURN_FALSE_IF_EGL_ERROR(
      success = eglChooseConfig(display, configuration_attributes,
                                &frame_buffer_configuration,
                                kRequestedNumConfigs, &returned_num_configs));
  if (returned_num_configs != kRequestedNumConfigs || !success) return false;

  // Create a pixel buffer surface.
  EGLint pixel_buffer_attributes[] = {
      EGL_WIDTH, pixel_buffer_width, EGL_HEIGHT, pixel_buffer_height, EGL_NONE,
  };

  RETURN_FALSE_IF_EGL_ERROR(
      pixel_buffer_surface = eglCreatePbufferSurface(
          display, frame_buffer_configuration, pixel_buffer_attributes));
  if (pixel_buffer_surface == EGL_NO_SURFACE) return false;
  auto surface_cleanup = MakeCleanup([display, pixel_buffer_surface]() {
    eglDestroySurface(display, pixel_buffer_surface);
  });

  // Create the EGL rendering context.
  RETURN_FALSE_IF_EGL_ERROR(
      context = eglCreateContext(display, frame_buffer_configuration,
                                 EGL_NO_CONTEXT, context_attributes));
  if (context == EGL_NO_CONTEXT) return false;

  initialize_cleanup.release();
  surface_cleanup.release();
  *egl_offscreen_context = std::unique_ptr<EGLOffscreenContext>(
      new EGLOffscreenContext(context, display, pixel_buffer_surface));
  return true;
}

bool EGLOffscreenContext::MakeCurrent() const {
  RETURN_FALSE_IF_EGL_ERROR(eglMakeCurrent(display_, pixel_buffer_surface_,
                                           pixel_buffer_surface_, context_));
  return true;
}

bool EGLOffscreenContext::Release() {
  if (context_ != EGL_NO_CONTEXT && context_ == eglGetCurrentContext())
    RETURN_FALSE_IF_EGL_ERROR(eglMakeCurrent(display_, EGL_NO_SURFACE,
                                             EGL_NO_SURFACE, EGL_NO_CONTEXT));
  return true;
}
