// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_GL_IMPELLER_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_GL_IMPELLER_H_

#include "flutter/fml/macros.h"
#include "flutter/impeller/renderer/context.h"
#include "flutter/shell/gpu/gpu_surface_gl_delegate.h"
#include "flutter/shell/platform/ohos/ohos_context_gl_impeller.h"
#include "flutter/shell/platform/ohos/surface/ohos_native_window.h"
#include "flutter/shell/platform/ohos/surface/ohos_surface.h"

namespace flutter {

class OHOSSurfaceGLImpeller final : public GPUSurfaceGLDelegate,
                                    public OHOSSurface {
 public:
  OHOSSurfaceGLImpeller(
      const std::shared_ptr<OHOSContextGLImpeller>& ohos_context);

  ~OHOSSurfaceGLImpeller() override;

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
  std::unique_ptr<Surface> CreateSnapshotSurface() override;

  // |OHOSSurface|
  std::shared_ptr<impeller::Context> GetImpellerContext() override;

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

 private:
  std::shared_ptr<OHOSContextGLImpeller> ohos_context_;
  std::unique_ptr<impeller::egl::Surface> onscreen_surface_;
  std::unique_ptr<impeller::egl::Surface> offscreen_surface_;
  fml::RefPtr<OHOSNativeWindow> native_window_;

  bool is_valid_ = false;

  bool OnGLContextMakeCurrent();

  bool RecreateOnscreenSurfaceAndMakeOnscreenContextCurrent();

  FML_DISALLOW_COPY_AND_ASSIGN(OHOSSurfaceGLImpeller);
};

}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_GL_IMPELLER_H_
