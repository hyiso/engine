// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_GL_SKIA_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_GL_SKIA_H_

#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/shell/gpu/gpu_surface_gl_skia.h"
#include "flutter/shell/platform/ohos/napi/platform_view_ohos_napi.h"
#include "flutter/shell/platform/ohos/ohos_context_gl_skia.h"
#include "flutter/shell/platform/ohos/ohos_environment_gl.h"
#include "flutter/shell/platform/ohos/surface/ohos_surface.h"

namespace flutter {

class OHOSSurfaceGLSkia final : public GPUSurfaceGLDelegate,
                                public OHOSSurface {
 public:
  OHOSSurfaceGLSkia(const std::shared_ptr<OHOSContextGLSkia>& ohos_context);

  ~OHOSSurfaceGLSkia() override;

  // |OHOSSurface|
  bool IsValid() const override;

  // |OHOSSurface|
  std::unique_ptr<Surface> CreateGPUSurface(
      GrDirectContext* gr_context) override;

  // |OHOSSurface|
  void TeardownOnScreenContext() override;

  // |OHOSSurface|
  bool OnScreenSurfaceResize(const SkISize& size) override;

  // |OHOSSurface|
  bool ResourceContextMakeCurrent() override;

  // |OHOSSurface|
  bool ResourceContextClearCurrent() override;

  // |OHOSSurface|
  bool SetNativeWindow(fml::RefPtr<OHOSNativeWindow> window) override;

  // |OHOSSurface|
  virtual std::unique_ptr<Surface> CreateSnapshotSurface() override;

  // |GPUSurfaceGLDelegate|
  std::unique_ptr<GLContextResult> GLContextMakeCurrent() override;

  // |GPUSurfaceGLDelegate|
  bool GLContextClearCurrent() override;

  // |GPUSurfaceGLDelegate|
  SurfaceFrame::FramebufferInfo GLContextFramebufferInfo() const override;

  // |GPUSurfaceGLDelegate|
  void GLContextSetDamageRegion(const std::optional<SkIRect>& region) override;

  // |GPUSurfaceGLDelegate|
  bool GLContextPresent(const GLPresentInfo& present_info) override;

  // |GPUSurfaceGLDelegate|
  GLFBOInfo GLContextFBO(GLFrameInfo frame_info) const override;

  // |GPUSurfaceGLDelegate|
  sk_sp<const GrGLInterface> GetGLInterface() const override;

  // Obtain a raw pointer to the on-screen OHOSEGLSurface.
  //
  // This method is intended for use in tests. Callers must not
  // delete the returned pointer.
  OHOSEGLSurface* GetOnscreenSurface() const { return onscreen_surface_.get(); }

 private:
  std::shared_ptr<OHOSContextGLSkia> ohos_context_;
  fml::RefPtr<OHOSNativeWindow> native_window_;
  std::unique_ptr<OHOSEGLSurface> onscreen_surface_;
  std::unique_ptr<OHOSEGLSurface> offscreen_surface_;

  FML_DISALLOW_COPY_AND_ASSIGN(OHOSSurfaceGLSkia);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_GL_SKIA_H_
