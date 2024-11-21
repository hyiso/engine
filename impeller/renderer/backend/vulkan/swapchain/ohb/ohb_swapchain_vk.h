// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_SWAPCHAIN_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_SWAPCHAIN_VK_H_

#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_swapchain_impl_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain/swapchain_vk.h"
#include "impeller/toolkit/ohos/native_window.h"

namespace impeller {

//------------------------------------------------------------------------------
/// @brief      The implementation of a swapchain that uses hardware buffers
///             presented to a given surface control on OHOS.
///
class OHBSwapchainVK final : public SwapchainVK {
 public:
  // |SwapchainVK|
  ~OHBSwapchainVK() override;

  OHBSwapchainVK(const OHBSwapchainVK&) = delete;

  OHBSwapchainVK& operator=(const OHBSwapchainVK&) = delete;

  // |SwapchainVK|
  bool IsValid() const override;

  // |SwapchainVK|
  std::unique_ptr<Surface> AcquireNextDrawable() override;

  // |SwapchainVK|
  vk::Format GetSurfaceFormat() const override;

  // |SwapchainVK|
  void UpdateSurfaceSize(const ISize& size) override;

 private:
  friend class SwapchainVK;

  std::weak_ptr<Context> context_;
  const bool enable_msaa_;
  size_t swapchain_image_count_ = 3u;
  std::shared_ptr<OHBSwapchainImplVK> impl_;

  explicit OHBSwapchainVK(const std::shared_ptr<Context>& context,
                          OHNativeWindow* window,
                          const vk::UniqueSurfaceKHR& surface,
                          const ISize& size,
                          bool enable_msaa);
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_SWAPCHAIN_VK_H_
