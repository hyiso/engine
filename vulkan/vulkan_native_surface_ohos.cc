// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vulkan_native_surface_ohos.h"

namespace vulkan {

VulkanNativeSurfaceOHOS::VulkanNativeSurfaceOHOS(OHNativeWindow* native_window)
    : native_window_(native_window) {
  if (native_window_ == nullptr) {
    return;
  }
}

VulkanNativeSurfaceOHOS::~VulkanNativeSurfaceOHOS() {
  if (native_window_ == nullptr) {
    return;
  }
  OH_NativeWindow_DestroyNativeWindow(native_window_);
}

const char* VulkanNativeSurfaceOHOS::GetExtensionName() const {
  return VK_OHOS_SURFACE_EXTENSION_NAME;
}

VkSurfaceKHR VulkanNativeSurfaceOHOS::CreateSurfaceHandle(
    VulkanProcTable& vk,
    const VulkanHandle<VkInstance>& instance) const {
  if (!vk.IsValid() || !instance) {
    return VK_NULL_HANDLE;
  }

  const VkSurfaceCreateInfoOHOS create_info = {
      .sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS,
      .pNext = nullptr,
      .flags = 0,
      .window = native_window_,
  };

  VkSurfaceKHR surface = VK_NULL_HANDLE;

  if (VK_CALL_LOG_ERROR(vk.CreateSurfaceOHOS(instance, &create_info, nullptr,
                                             &surface)) != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  return surface;
}

bool VulkanNativeSurfaceOHOS::IsValid() const {
  return native_window_ != nullptr;
}

SkISize VulkanNativeSurfaceOHOS::GetSize() const {
  int32_t width = 0;
  int32_t height = 0;
  if (native_window_ != nullptr) {
    OH_NativeWindow_NativeWindowHandleOpt(native_window_, GET_BUFFER_GEOMETRY,
                                          &height, &width);
  }
  return SkISize::Make(width, height);
}

}  // namespace vulkan
