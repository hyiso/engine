// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_VULKAN_IMPELLER_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_VULKAN_IMPELLER_H_

#include "flutter/fml/concurrent_message_loop.h"
#include "flutter/fml/macros.h"
#include "flutter/impeller/renderer/backend/vulkan/surface_context_vk.h"
#include "flutter/shell/gpu/gpu_surface_vulkan_impeller.h"
#include "flutter/shell/platform/ohos/ohos_context_vulkan_impeller.h"
#include "flutter/shell/platform/ohos/surface/ohos_native_window.h"
#include "flutter/shell/platform/ohos/surface/ohos_surface.h"

namespace flutter {

class OHOSSurfaceVulkanImpeller : public OHOSSurface {
 public:
  OHOSSurfaceVulkanImpeller(
      const std::shared_ptr<OHOSContextVulkanImpeller>& ohos_context);

  ~OHOSSurfaceVulkanImpeller() override;

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
  std::shared_ptr<impeller::Context> GetImpellerContext() override;

  // |OHOSSurface|
  bool SetNativeWindow(fml::RefPtr<OHOSNativeWindow> window) override;

 private:
  std::shared_ptr<impeller::SurfaceContextVK> surface_context_vk_;
  fml::RefPtr<OHOSNativeWindow> native_window_;
  // The first GPU Surface is initialized as soon as the
  // OHOSSurfaceVulkanImpeller is created. This ensures that the pipelines
  // are bootstrapped as early as possible.
  std::unique_ptr<GPUSurfaceVulkanImpeller> eager_gpu_surface_;

  bool is_valid_ = false;

  FML_DISALLOW_COPY_AND_ASSIGN(OHOSSurfaceVulkanImpeller);
};
}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SURFACE_VULKAN_IMPELLER_H_
