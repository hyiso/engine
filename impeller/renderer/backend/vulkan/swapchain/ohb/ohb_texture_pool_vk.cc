// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/swapchain/ohb/ohb_texture_pool_vk.h"

#include "flutter/fml/trace_event.h"

namespace impeller {

OHBTexturePoolVK::OHBTexturePoolVK(std::weak_ptr<Context> context,
                                   ohos::NativeBufferConfig config,
                                   size_t max_entries)
    : context_(std::move(context)), config_(config), max_entries_(max_entries) {
  for (auto i = 0u; i < max_entries_; i++) {
    pool_.emplace_back(CreateTexture());
  }
  is_valid_ = true;
}

OHBTexturePoolVK::~OHBTexturePoolVK() = default;

OHBTexturePoolVK::PoolEntry OHBTexturePoolVK::Pop() {
  {
    Lock lock(pool_mutex_);
    if (!pool_.empty()) {
      // Buffers are pushed to the back of the queue. To give the ready fences
      // the most time to signal, pick a buffer from the front of the queue.
      auto entry = pool_.front();
      pool_.pop_front();
      return entry;
    }
  }
  return PoolEntry{CreateTexture()};
}

void OHBTexturePoolVK::Push(std::shared_ptr<OHBTextureSourceVK> texture,
                            fml::UniqueFD render_ready_fence) {
  if (!texture) {
    return;
  }
  Lock lock(pool_mutex_);
  pool_.push_back(PoolEntry{std::move(texture), std::move(render_ready_fence)});
  PerformGCLocked();
}

std::shared_ptr<OHBTextureSourceVK> OHBTexturePoolVK::CreateTexture() const {
  TRACE_EVENT0("impeller", "CreateSwapchainTexture");
  auto context = context_.lock();
  if (!context) {
    VALIDATION_LOG << "Context died before image could be created.";
    return nullptr;
  }

  auto ohb = std::make_unique<ohos::NativeBuffer>(config_);
  if (!ohb->IsValid()) {
    VALIDATION_LOG << "Could not create hardware buffer of size: "
                   << config_.size;
    return nullptr;
  }

  auto ohb_texture_source =
      std::make_shared<OHBTextureSourceVK>(context, std::move(ohb), true);
  if (!ohb_texture_source->IsValid()) {
    VALIDATION_LOG << "Could not create hardware buffer texture source for "
                      "swapchain image of size: "
                   << config_.size;
    return nullptr;
  }

  return ohb_texture_source;
}

void OHBTexturePoolVK::PerformGC() {
  Lock lock(pool_mutex_);
  PerformGCLocked();
}

void OHBTexturePoolVK::PerformGCLocked() {
  while (!pool_.empty() && (pool_.size() > max_entries_)) {
    // Buffers are pushed to the back of the queue and popped from the front.
    // The ones at the back should be given the most time for their fences to
    // signal. If we are going to get rid of textures, they might as well be the
    // newest ones since their fences will take the longest to signal.
    pool_.pop_back();
  }
}

bool OHBTexturePoolVK::IsValid() const {
  return is_valid_;
}

}  // namespace impeller
