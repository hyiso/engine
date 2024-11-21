// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_swapchain_vk.h"

#include "flutter/fml/trace_event.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_formats.h"

#include "vulkan/vulkan_enums.hpp"

namespace impeller {

OHBSwapchainVK::OHBSwapchainVK(const std::shared_ptr<Context>& context,
                               OHNativeWindow* window,
                               const vk::UniqueSurfaceKHR& surface,
                               const ISize& size,
                               bool enable_msaa)
    : context_(context), enable_msaa_(enable_msaa) {
  const auto [caps_result, surface_caps] =
      ContextVK::Cast(*context).GetPhysicalDevice().getSurfaceCapabilitiesKHR(
          *surface);
  if (caps_result == vk::Result::eSuccess) {
    swapchain_image_count_ =
        std::clamp(surface_caps.minImageCount + 1u,  // preferred image count
                   surface_caps.minImageCount,       // min count cannot be zero
                   surface_caps.maxImageCount == 0u
                       ? surface_caps.minImageCount + 1u
                       : surface_caps.maxImageCount  // max zero means no limit
        );
  }

  UpdateSurfaceSize(size);
}

OHBSwapchainVK::~OHBSwapchainVK() = default;

// |SwapchainVK|
bool OHBSwapchainVK::IsValid() const {
  return impl_ ? impl_->IsValid() : false;
}

// |SwapchainVK|
std::unique_ptr<Surface> OHBSwapchainVK::AcquireNextDrawable() {
  if (!IsValid()) {
    return nullptr;
  }

  TRACE_EVENT0("impeller", __FUNCTION__);
  return impl_->AcquireNextDrawable();
}

// |SwapchainVK|
vk::Format OHBSwapchainVK::GetSurfaceFormat() const {
  return IsValid() ? ToVKImageFormat(ToPixelFormat(impl_->GetConfig().format))
                   : vk::Format::eUndefined;
}

// |SwapchainVK|
void OHBSwapchainVK::UpdateSurfaceSize(const ISize& size) {
  if (impl_ && impl_->GetSize() == size) {
    return;
  }
  TRACE_EVENT0("impeller", __FUNCTION__);
  auto impl = OHBSwapchainImplVK::Create(context_,               //
                                         size,                   //
                                         enable_msaa_,           //
                                         swapchain_image_count_  //
  );
  if (!impl || !impl->IsValid()) {
    VALIDATION_LOG << "Could not resize swapchain to size: " << size;
    return;
  }
  impl_ = std::move(impl);
}

}  // namespace impeller
