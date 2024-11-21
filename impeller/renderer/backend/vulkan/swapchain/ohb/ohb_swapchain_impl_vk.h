// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_SWAPCHAIN_IMPL_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_SWAPCHAIN_IMPL_VK_H_

#include <memory>

#include "flutter/fml/closure.h"
#include "flutter/fml/synchronization/semaphore.h"
#include "impeller/base/thread.h"
#include "impeller/renderer/backend/vulkan/ohos/ohb_texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain/ohb/external_fence_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_texture_pool_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain/swapchain_transients_vk.h"
#include "impeller/renderer/surface.h"
#include "impeller/toolkit/ohos/native_buffer.h"

namespace impeller {

//------------------------------------------------------------------------------
/// @brief      The implementation of a swapchain at a specific size. Resizes to
///             the surface will cause the instance of the swapchain impl at
///             that size to be discarded along with all its caches and
///             transients.
///
class OHBSwapchainImplVK final
    : public std::enable_shared_from_this<OHBSwapchainImplVK> {
 public:
  //----------------------------------------------------------------------------
  /// @brief      Create a swapchain of a specific size whose images will be
  ///             presented to the provided surface control.
  ///
  /// @param[in]  context          The context whose allocators will be used to
  ///                              create swapchain image resources.
  /// @param[in]  surface_control  The surface control to which the swapchain
  ///                              images will be presented.
  /// @param[in]  size             The size of the swapchain images. This is
  ///                              constant for the lifecycle of the swapchain
  ///                              impl.
  /// @param[in]  enable_msaa      If the swapchain images will be presented
  ///                              using a render target that enables MSAA. This
  ///                              allows for additional caching of transients.
  ///
  /// @return     A valid swapchain impl if one can be created. `nullptr`
  ///             otherwise.
  ///
  static std::shared_ptr<OHBSwapchainImplVK> Create(
      const std::weak_ptr<Context>& context,
      const ISize& size,
      bool enable_msaa,
      size_t swapchain_image_count);

  ~OHBSwapchainImplVK();

  OHBSwapchainImplVK(const OHBSwapchainImplVK&) = delete;

  OHBSwapchainImplVK& operator=(const OHBSwapchainImplVK&) = delete;

  //----------------------------------------------------------------------------
  /// @return     The size of the swapchain images that will be displayed on the
  ///             surface control.
  ///
  const ISize& GetSize() const;

  //----------------------------------------------------------------------------
  /// @return     If the swapchain impl is valid. If it is not, the instance
  ///             must be discarded. There is no error recovery.
  ///
  bool IsValid() const;

  //----------------------------------------------------------------------------
  /// @brief      Get the descriptor used to create the native buffers that
  ///             will be displayed on the surface control.
  ///
  /// @return     The descriptor.
  ///
  const ohos::NativeBufferConfig& GetConfig() const;

  //----------------------------------------------------------------------------
  /// @brief      Acquire the next surface that can be used to present to the
  ///             swapchain.
  ///
  /// @return     A surface if one can be created. If one cannot be created, it
  ///             is likely due to resource exhaustion.
  ///
  std::unique_ptr<Surface> AcquireNextDrawable();

 private:
  using AutoSemaSignaler = std::shared_ptr<fml::ScopedCleanupClosure>;

  ohos::NativeBufferConfig config_;
  std::shared_ptr<OHBTexturePoolVK> pool_;
  std::shared_ptr<SwapchainTransientsVK> transients_;
  // In C++20, this mutex can be replaced by the shared pointer specialization
  // of std::atomic.
  Mutex currently_displayed_texture_mutex_;
  std::shared_ptr<OHBTextureSourceVK> currently_displayed_texture_
      IPLR_GUARDED_BY(currently_displayed_texture_mutex_);
  std::shared_ptr<fml::Semaphore> pending_presents_;
  bool is_valid_ = false;

  explicit OHBSwapchainImplVK(const std::weak_ptr<Context>& context,
                              const ISize& size,
                              bool enable_msaa,
                              size_t swapchain_image_count);

  bool Present(const AutoSemaSignaler& signaler,
               const std::shared_ptr<OHBTextureSourceVK>& texture);

  vk::UniqueSemaphore CreateRenderReadySemaphore(
      const std::shared_ptr<fml::UniqueFD>& fd) const;

  bool SubmitWaitForRenderReady(
      const std::shared_ptr<fml::UniqueFD>& render_ready_fence,
      const std::shared_ptr<OHBTextureSourceVK>& texture) const;

  std::shared_ptr<ExternalFenceVK> SubmitSignalForPresentReady(
      const std::shared_ptr<OHBTextureSourceVK>& texture) const;

  //   void OnTextureUpdatedOnSurfaceControl(
  //       const AutoSemaSignaler& signaler,
  //       std::shared_ptr<OHBTextureSourceVK> texture,
  //       ASurfaceTransactionStats* stats);
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_SWAPCHAIN_IMPL_VK_H_
