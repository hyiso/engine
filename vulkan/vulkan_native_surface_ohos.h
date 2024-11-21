// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_VULKAN_VULKAN_NATIVE_SURFACE_OHOS_H_
#define FLUTTER_VULKAN_VULKAN_NATIVE_SURFACE_OHOS_H_

#include <native_window/external_window.h>

#include "flutter/fml/macros.h"
#include "vulkan_native_surface.h"

namespace vulkan {

class VulkanNativeSurfaceOHOS : public VulkanNativeSurface {
 public:
  /// Create a native surface from the valid OHNativeWindow reference. Ownership
  /// of the OHNativeWindow is assumed by this instance.
  explicit VulkanNativeSurfaceOHOS(OHNativeWindow* native_window);

  ~VulkanNativeSurfaceOHOS();

  const char* GetExtensionName() const override;

  VkSurfaceKHR CreateSurfaceHandle(
      VulkanProcTable& vk,
      const VulkanHandle<VkInstance>& instance) const override;

  bool IsValid() const override;

  SkISize GetSize() const override;

 private:
  OHNativeWindow* native_window_;

  FML_DISALLOW_COPY_AND_ASSIGN(VulkanNativeSurfaceOHOS);
};

}  // namespace vulkan

#endif  // FLUTTER_VULKAN_VULKAN_NATIVE_SURFACE_OHOS_H_
