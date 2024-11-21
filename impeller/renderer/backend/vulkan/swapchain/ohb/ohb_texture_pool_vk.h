// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_TEXTURE_POOL_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_TEXTURE_POOL_VK_H_

#include <deque>

#include "flutter/fml/unique_fd.h"
#include "impeller/base/thread.h"
#include "impeller/renderer/backend/vulkan/ohos/ohb_texture_source_vk.h"

namespace impeller {

//------------------------------------------------------------------------------
/// @brief      Maintains a bounded pool of native buffer backed texture
///             sources that can be used as swapchain images.
///
///             The number of cached entries in the texture pool is capped to a
///             caller specified value.
///
///             If a previously cached entry cannot be obtained from the pool, a
///             new entry is created. The only case where a valid texture source
///             cannot be obtained is due to resource exhaustion.
///
///             Pools are thread-safe.
///
class OHBTexturePoolVK {
 public:
  struct PoolEntry {
    std::shared_ptr<OHBTextureSourceVK> texture;
    std::shared_ptr<fml::UniqueFD> render_ready_fence;

    explicit PoolEntry(std::shared_ptr<OHBTextureSourceVK> p_item,
                       fml::UniqueFD p_render_ready_fence = {})
        : texture(std::move(p_item)),
          render_ready_fence(std::make_shared<fml::UniqueFD>(
              std::move(p_render_ready_fence))) {}

    constexpr bool IsValid() const { return !!texture; }
  };

  //----------------------------------------------------------------------------
  /// @brief      Create a new (empty) texture pool.
  ///
  /// @param[in]  context        The context whose allocators will be used to
  ///                            create the resources for the texture sources.
  /// @param[in]  config         The config of the native buffers that
  ///                            will be used to create the backing stores of
  ///                            the texture sources.
  /// @param[in]  max_entries    The maximum entries that will remain cached
  ///                            in the pool.
  ///
  explicit OHBTexturePoolVK(std::weak_ptr<Context> context,
                            ohos::NativeBufferConfig config,
                            size_t max_entries = 3u);

  ~OHBTexturePoolVK();

  OHBTexturePoolVK(const OHBTexturePoolVK&) = delete;

  OHBTexturePoolVK& operator=(const OHBTexturePoolVK&) = delete;

  //----------------------------------------------------------------------------
  /// @brief      If the pool can create and pool native buffer backed texture
  ///             sources. The only reason valid textures cannot be obtained
  ///             from a valid pool is because of resource exhaustion.
  ///
  /// @return     `true` if valid, `false` otherwise.
  ///
  bool IsValid() const;

  //----------------------------------------------------------------------------
  /// @brief      Pops an texture source from the pool. If the pool is empty, a
  ///             new texture source is created and returned.
  ///
  ///             This operation is thread-safe.
  ///
  /// @return     A texture source that can be used as a swapchain image. This
  ///             can be nullptr in case of resource exhaustion.
  ///
  PoolEntry Pop();

  //----------------------------------------------------------------------------
  /// @brief      Push a popped texture back into the pool. This also performs a
  ///             GC.
  ///
  ///             This operation is thread-safe.
  ///
  /// @warning    Only a texture source obtained from the same pool can be
  ///             returned to it. It is user error to mix and match texture
  ///             sources from different pools.
  ///
  /// @param[in]  texture  The texture to be returned to the pool.
  ///
  void Push(std::shared_ptr<OHBTextureSourceVK> texture,
            fml::UniqueFD render_ready_fence);

  //----------------------------------------------------------------------------
  /// @brief      Perform an explicit GC of the pool items. This happens
  ///             implicitly when a texture source us pushed into the pool but
  ///             one may be necessary explicitly if there is no push back into
  ///             the pool for a long time.
  ///
  void PerformGC();

 private:
  const std::weak_ptr<Context> context_;
  const ohos::NativeBufferConfig config_;
  const size_t max_entries_;
  bool is_valid_ = false;
  Mutex pool_mutex_;
  std::deque<PoolEntry> pool_ IPLR_GUARDED_BY(pool_mutex_);

  void PerformGCLocked() IPLR_REQUIRES(pool_mutex_);

  std::shared_ptr<OHBTextureSourceVK> CreateTexture() const;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_TEXTURE_POOL_VK_H_
