// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_swapchain_impl_vk.h"

#include "flutter/fml/trace_event.h"
#include "impeller/base/validation.h"
#include "impeller/renderer/backend/vulkan/barrier_vk.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/command_encoder_vk.h"
#include "impeller/renderer/backend/vulkan/fence_waiter_vk.h"
#include "impeller/renderer/backend/vulkan/gpu_tracer_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_formats.h"
#include "impeller/renderer/backend/vulkan/swapchain/surface_vk.h"

namespace impeller {

//------------------------------------------------------------------------------
/// The maximum number of presents pending in the compositor after which the
/// acquire calls will block. This value is 2 images given to the system
/// compositor and one for the raster thread, Because the semaphore is acquired
/// when the CPU Begins working on the texture
///
static constexpr const size_t kMaxPendingPresents = 3u;

static TextureDescriptor ToSwapchainTextureDescriptor(
    const ohos::NativeBufferConfig& config) {
  TextureDescriptor desc;
  desc.storage_mode = StorageMode::kDevicePrivate;
  desc.type = TextureType::kTexture2D;
  desc.format = ToPixelFormat(config.format);
  desc.size = config.size;
  desc.mip_count = 1u;
  desc.usage = TextureUsage::kRenderTarget;
  desc.sample_count = SampleCount::kCount1;
  desc.compression_type = CompressionType::kLossless;
  return desc;
}

std::shared_ptr<OHBSwapchainImplVK> OHBSwapchainImplVK::Create(
    const std::weak_ptr<Context>& context,
    const ISize& size,
    bool enable_msaa,
    size_t swapchain_image_count) {
  auto impl = std::shared_ptr<OHBSwapchainImplVK>(new OHBSwapchainImplVK(
      context, size, enable_msaa, swapchain_image_count));
  return impl->IsValid() ? impl : nullptr;
}

OHBSwapchainImplVK::OHBSwapchainImplVK(const std::weak_ptr<Context>& context,
                                       const ISize& size,
                                       bool enable_msaa,
                                       size_t swapchain_image_count)
    : pending_presents_(std::make_shared<fml::Semaphore>(kMaxPendingPresents)) {
  config_ = ohos::NativeBufferConfig::MakeForSwapchainImage(size);
  pool_ = std::make_shared<OHBTexturePoolVK>(context, config_,
                                             swapchain_image_count);
  if (!pool_->IsValid()) {
    return;
  }
  transients_ = std::make_shared<SwapchainTransientsVK>(
      context, ToSwapchainTextureDescriptor(config_), enable_msaa);
  is_valid_ = true;
}

OHBSwapchainImplVK::~OHBSwapchainImplVK() = default;

const ISize& OHBSwapchainImplVK::GetSize() const {
  return config_.size;
}

bool OHBSwapchainImplVK::IsValid() const {
  return is_valid_;
}

const ohos::NativeBufferConfig& OHBSwapchainImplVK::GetConfig() const {
  return config_;
}

std::unique_ptr<Surface> OHBSwapchainImplVK::AcquireNextDrawable() {
  {
    TRACE_EVENT0("impeller", "CompositorPendingWait");
    if (!pending_presents_->Wait()) {
      return nullptr;
    }
  }

  AutoSemaSignaler auto_sema_signaler =
      std::make_shared<fml::ScopedCleanupClosure>(
          [sema = pending_presents_]() { sema->Signal(); });

  if (!is_valid_) {
    return nullptr;
  }

  auto pool_entry = pool_->Pop();

  if (!pool_entry.IsValid()) {
    VALIDATION_LOG << "Could not create OHB texture source.";
    return nullptr;
  }

  auto context = transients_->GetContext().lock();
  if (context) {
    ContextVK::Cast(*context).GetGPUTracer()->MarkFrameStart();
  }

  // Ask the GPU to wait for the render ready semaphore to be signaled before
  // performing rendering operations.
  if (!SubmitWaitForRenderReady(pool_entry.render_ready_fence,
                                pool_entry.texture)) {
    VALIDATION_LOG << "Could not submit a command to the GPU to wait on render "
                      "readiness.";
    return nullptr;
  }

  auto surface = SurfaceVK::WrapSwapchainImage(
      transients_, pool_entry.texture,
      [signaler = auto_sema_signaler, weak = weak_from_this(),
       texture = pool_entry.texture]() {
        auto thiz = weak.lock();
        if (!thiz) {
          VALIDATION_LOG << "Swapchain died before image could be presented.";
          return false;
        }
        return thiz->Present(signaler, texture);
      });

  if (!surface) {
    return nullptr;
  }

  return surface;
}

bool OHBSwapchainImplVK::Present(
    const AutoSemaSignaler& signaler,
    const std::shared_ptr<OHBTextureSourceVK>& texture) {
  auto context = transients_->GetContext().lock();
  if (context) {
    ContextVK::Cast(*context).GetGPUTracer()->MarkFrameEnd();
  }

  if (!texture) {
    return false;
  }

  auto fence = SubmitSignalForPresentReady(texture);

  if (!fence) {
    VALIDATION_LOG << "Could not submit completion signal.";
    return false;
  }
  return false;

  // ohos::SurfaceTransaction transaction;
  // if (!transaction.SetContents(control.get(),               //
  //                              texture->GetBackingStore(),  //
  //                              fence->CreateFD()            //
  //                              )) {
  //   VALIDATION_LOG << "Could not set swapchain image contents on the surface
  //   "
  //                     "control.";
  //   return false;
  // }
  // return transaction.Apply([signaler, texture, weak = weak_from_this()](
  //                              ASurfaceTransactionStats* stats) {
  //   auto thiz = weak.lock();
  //   if (!thiz) {
  //     return;
  //   }
  //   thiz->OnTextureUpdatedOnSurfaceControl(signaler, texture, stats);
  // });
}

std::shared_ptr<ExternalFenceVK>
OHBSwapchainImplVK::SubmitSignalForPresentReady(
    const std::shared_ptr<OHBTextureSourceVK>& texture) const {
  auto context = transients_->GetContext().lock();
  if (!context) {
    return nullptr;
  }
  auto fence = std::make_shared<ExternalFenceVK>(context);
  if (!fence || !fence->IsValid()) {
    return nullptr;
  }

  auto command_buffer = context->CreateCommandBuffer();
  if (!command_buffer) {
    return nullptr;
  }
  command_buffer->SetLabel("OHBSubmitSignalForPresentReadyCB");
  const auto& encoder = CommandBufferVK::Cast(*command_buffer).GetEncoder();

  const auto command_encoder_vk = encoder->GetCommandBuffer();

  BarrierVK barrier;
  barrier.cmd_buffer = command_encoder_vk;
  barrier.new_layout = vk::ImageLayout::eGeneral;
  barrier.src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  barrier.src_access = vk::AccessFlagBits::eColorAttachmentWrite;
  barrier.dst_stage = vk::PipelineStageFlagBits::eBottomOfPipe;
  barrier.dst_access = {};

  if (!texture->SetLayout(barrier).ok()) {
    return nullptr;
  }

  encoder->Track(fence->GetSharedHandle());

  if (!encoder->EndCommandBuffer()) {
    return nullptr;
  }

  vk::SubmitInfo submit_info;
  submit_info.setCommandBuffers(command_encoder_vk);

  auto result = ContextVK::Cast(*context).GetGraphicsQueue()->Submit(
      submit_info, fence->GetHandle());
  if (result != vk::Result::eSuccess) {
    return nullptr;
  }
  return fence;
}

vk::UniqueSemaphore OHBSwapchainImplVK::CreateRenderReadySemaphore(
    const std::shared_ptr<fml::UniqueFD>& fd) const {
  if (!fd->is_valid()) {
    return {};
  }

  auto context = transients_->GetContext().lock();
  if (!context) {
    return {};
  }

  const auto& context_vk = ContextVK::Cast(*context);
  const auto& device = context_vk.GetDevice();

  auto signal_wait = device.createSemaphoreUnique({});

  if (signal_wait.result != vk::Result::eSuccess) {
    return {};
  }

  context_vk.SetDebugName(*signal_wait.value, "OHBRenderReadySemaphore");

  vk::ImportSemaphoreFdInfoKHR import_info;
  import_info.semaphore = *signal_wait.value;
  import_info.fd = fd->get();
  import_info.handleType = vk::ExternalSemaphoreHandleTypeFlagBits::eSyncFd;
  // From the spec: Sync FDs can only be imported temporarily.
  import_info.flags = vk::SemaphoreImportFlagBitsKHR::eTemporary;

  const auto import_result = device.importSemaphoreFdKHR(import_info);

  if (import_result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not import semaphore FD: "
                   << vk::to_string(import_result);
    return {};
  }

  // From the spec: Importing a semaphore payload from a file descriptor
  // transfers ownership of the file descriptor from the application to the
  // Vulkan implementation. The application must not perform any operations on
  // the file descriptor after a successful import.
  [[maybe_unused]] auto released = fd->release();

  return std::move(signal_wait.value);
}

bool OHBSwapchainImplVK::SubmitWaitForRenderReady(
    const std::shared_ptr<fml::UniqueFD>& render_ready_fence,
    const std::shared_ptr<OHBTextureSourceVK>& texture) const {
  // If there is no render ready fence, we are already ready to render into
  // the texture. There is nothing more to do.
  if (!render_ready_fence || !render_ready_fence->is_valid()) {
    return true;
  }

  auto context = transients_->GetContext().lock();
  if (!context) {
    return false;
  }

  auto completion_fence =
      ContextVK::Cast(*context).GetDevice().createFenceUnique({}).value;
  if (!completion_fence) {
    return false;
  }

  auto command_buffer = context->CreateCommandBuffer();
  if (!command_buffer) {
    return false;
  }
  command_buffer->SetLabel("OHBSubmitWaitForRenderReadyCB");
  const auto& encoder = CommandBufferVK::Cast(*command_buffer).GetEncoder();

  const auto command_buffer_vk = encoder->GetCommandBuffer();

  BarrierVK barrier;
  barrier.cmd_buffer = command_buffer_vk;
  barrier.new_layout = vk::ImageLayout::eColorAttachmentOptimal;
  barrier.src_stage = vk::PipelineStageFlagBits::eBottomOfPipe;
  barrier.src_access = {};
  barrier.dst_stage = vk::PipelineStageFlagBits::eTopOfPipe;
  barrier.dst_access = {};

  if (!texture->SetLayout(barrier).ok()) {
    return false;
  }

  auto render_ready_semaphore =
      MakeSharedVK(CreateRenderReadySemaphore(render_ready_fence));
  encoder->Track(render_ready_semaphore);

  if (!encoder->EndCommandBuffer()) {
    return false;
  }

  vk::SubmitInfo submit_info;

  if (render_ready_semaphore) {
    static constexpr const auto kWaitStages =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eFragmentShader |
        vk::PipelineStageFlagBits::eTransfer;
    submit_info.setWaitSemaphores(render_ready_semaphore->Get());
    submit_info.setWaitDstStageMask(kWaitStages);
  }

  submit_info.setCommandBuffers(command_buffer_vk);

  auto result = ContextVK::Cast(*context).GetGraphicsQueue()->Submit(
      submit_info, *completion_fence);
  if (result != vk::Result::eSuccess) {
    return false;
  }

  ContextVK::Cast(*context).GetFenceWaiter()->AddFence(
      std::move(completion_fence), [encoder]() {});

  return true;
}

// void OHBSwapchainImplVK::OnTextureUpdatedOnSurfaceControl(
//     const AutoSemaSignaler& signaler,
//     std::shared_ptr<OHBTextureSourceVK> texture,
//     ASurfaceTransactionStats* stats) {
//   auto control = surface_control_.lock();
//   if (!control) {
//     return;
//   }

//   // Ask for an FD that gets signaled when the previous buffer is released.
//   This
//   // can be invalid if there is no wait necessary.
//   auto render_ready_fence =
//       ohos::CreatePreviousReleaseFence(*control, stats);

//   // The transaction completion indicates that the surface control now
//   // references the hardware buffer. We can recycle the previous set buffer
//   // safely.
//   Lock lock(currently_displayed_texture_mutex_);
//   auto old_texture = currently_displayed_texture_;
//   currently_displayed_texture_ = std::move(texture);
//   pool_->Push(std::move(old_texture), std::move(render_ready_fence));
// }

}  // namespace impeller
