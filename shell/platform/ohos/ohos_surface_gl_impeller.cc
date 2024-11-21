// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_surface_gl_impeller.h"

#include "flutter/fml/logging.h"
#include "flutter/impeller/toolkit/egl/surface.h"
#include "flutter/shell/gpu/gpu_surface_gl_impeller.h"

namespace flutter {

OHOSSurfaceGLImpeller::OHOSSurfaceGLImpeller(
    const std::shared_ptr<OHOSContextGLImpeller>& ohos_context)
    : ohos_context_(ohos_context) {
  offscreen_surface_ = ohos_context_->CreateOffscreenSurface();

  if (!offscreen_surface_) {
    FML_DLOG(ERROR) << "Could not create offscreen surface.";
    return;
  }

  // The onscreen surface will be acquired once the native window is set.

  is_valid_ = true;
}

OHOSSurfaceGLImpeller::~OHOSSurfaceGLImpeller() = default;

// |OHSOSurface|
bool OHOSSurfaceGLImpeller::IsValid() const {
  return is_valid_;
}

// |OHSOSurface|
std::unique_ptr<Surface> OHOSSurfaceGLImpeller::CreateGPUSurface(
    GrDirectContext* gr_context) {
  auto surface = std::make_unique<GPUSurfaceGLImpeller>(
      this,                                 // delegate
      ohos_context_->GetImpellerContext(),  // context
      true                                  // render_to_surface
  );
  if (!surface->IsValid()) {
    return nullptr;
  }
  return surface;
}

// |OHSOSurface|
void OHOSSurfaceGLImpeller::TeardownOnScreenContext() {
  GLContextClearCurrent();
  onscreen_surface_.reset();
}

// |OHSOSurface|
bool OHOSSurfaceGLImpeller::OnScreenSurfaceResize(const SkISize& size) {
  // The size is unused. It was added only for iOS where the sizes were
  // necessary to re-create auxiliary buffers (stencil, depth, etc.).
  return RecreateOnscreenSurfaceAndMakeOnscreenContextCurrent();
}

// |OHSOSurface|
bool OHOSSurfaceGLImpeller::ResourceContextMakeCurrent() {
  if (!offscreen_surface_) {
    return false;
  }
  return ohos_context_->ResourceContextMakeCurrent(offscreen_surface_.get());
}

// |OHSOSurface|
bool OHOSSurfaceGLImpeller::ResourceContextClearCurrent() {
  return ohos_context_->ResourceContextClearCurrent();
}

// |OHSOSurface|
bool OHOSSurfaceGLImpeller::SetNativeWindow(
    fml::RefPtr<OHOSNativeWindow> window) {
  native_window_ = std::move(window);
  return RecreateOnscreenSurfaceAndMakeOnscreenContextCurrent();
}

// |OHSOSurface|
std::unique_ptr<Surface> OHOSSurfaceGLImpeller::CreateSnapshotSurface() {
  FML_UNREACHABLE();
}

// |OHSOSurface|
std::shared_ptr<impeller::Context> OHOSSurfaceGLImpeller::GetImpellerContext() {
  return ohos_context_->GetImpellerContext();
}

// |GPUSurfaceGLDelegate|
std::unique_ptr<GLContextResult> OHOSSurfaceGLImpeller::GLContextMakeCurrent() {
  return std::make_unique<GLContextDefaultResult>(OnGLContextMakeCurrent());
}

bool OHOSSurfaceGLImpeller::OnGLContextMakeCurrent() {
  if (!onscreen_surface_) {
    return false;
  }

  return ohos_context_->OnscreenContextMakeCurrent(onscreen_surface_.get());
}

// |GPUSurfaceGLDelegate|
bool OHOSSurfaceGLImpeller::GLContextClearCurrent() {
  if (!onscreen_surface_) {
    return false;
  }

  return ohos_context_->OnscreenContextClearCurrent();
}

// |GPUSurfaceGLDelegate|
SurfaceFrame::FramebufferInfo OHOSSurfaceGLImpeller::GLContextFramebufferInfo()
    const {
  auto info = SurfaceFrame::FramebufferInfo{};
  info.supports_readback = true;
  info.supports_partial_repaint = false;
  return info;
}

// |GPUSurfaceGLDelegate|
void OHOSSurfaceGLImpeller::GLContextSetDamageRegion(
    const std::optional<SkIRect>& region) {
  // Not supported.
}

// |GPUSurfaceGLDelegate|
bool OHOSSurfaceGLImpeller::GLContextPresent(
    const GLPresentInfo& present_info) {
  if (!onscreen_surface_) {
    return false;
  }

  return onscreen_surface_->Present();
}

// |GPUSurfaceGLDelegate|
GLFBOInfo OHOSSurfaceGLImpeller::GLContextFBO(GLFrameInfo frame_info) const {
  // FBO0 is the default window bound framebuffer in EGL environments.
  return GLFBOInfo{
      .fbo_id = 0,
  };
}

// |GPUSurfaceGLDelegate|
sk_sp<const GrGLInterface> OHOSSurfaceGLImpeller::GetGLInterface() const {
  return nullptr;
}

bool OHOSSurfaceGLImpeller::
    RecreateOnscreenSurfaceAndMakeOnscreenContextCurrent() {
  GLContextClearCurrent();
  if (!native_window_) {
    return false;
  }
  onscreen_surface_.reset();
  auto onscreen_surface =
      ohos_context_->CreateOnscreenSurface(native_window_->handle());
  if (!onscreen_surface) {
    FML_DLOG(ERROR) << "Could not create onscreen surface.";
    return false;
  }
  onscreen_surface_ = std::move(onscreen_surface);
  return OnGLContextMakeCurrent();
}

}  // namespace flutter
