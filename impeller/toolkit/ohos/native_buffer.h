// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_TOOLKIT_OHOS_NATIVE_BUFFER_H_
#define FLUTTER_IMPELLER_TOOLKIT_OHOS_NATIVE_BUFFER_H_

#include <native_buffer/native_buffer.h>
#include <optional>

#include "flutter/fml/unique_fd.h"
#include "flutter/fml/unique_object.h"
#include "impeller/base/mask.h"
#include "impeller/geometry/size.h"

namespace impeller::ohos {

enum class NativeBufferFormat {
  //----------------------------------------------------------------------------
  /// This format is guaranteed to be supported on all versions of Android. This
  /// format can also be converted to an Impeller and Vulkan format.
  ///
  /// @see        Vulkan Format: VK_FORMAT_R8G8B8A8_UNORM
  /// @see        OpenGL ES Format: GL_RGBA8
  ///
  /// Why have many format when one format do trick?
  ///
  kR8G8B8A8UNormInt,
};

enum class NativeBufferUsageFlags {
  kNone = 0u,
  kCPURead = 1u << 0u,
  kCPUWrite = 1u << 1u,
  kMemDma = 1u << 2u,
  kHWRender = 1u << 3u,
  kHWTexture = 1u << 4u,
  kCPUReadOften = 1u << 5u,
  kAlignment512 = 1u << 6u,
};

using NativeBufferUsage = Mask<NativeBufferUsageFlags>;

//------------------------------------------------------------------------------
/// @brief      A config use to specify native buffer allocations.
///
struct NativeBufferConfig {
  NativeBufferFormat format = NativeBufferFormat::kR8G8B8A8UNormInt;
  ISize size;
  NativeBufferUsage usage = NativeBufferUsageFlags::kNone;

  //----------------------------------------------------------------------------
  /// @brief      Create a config of the given size that is suitable for use
  ///             as a swapchain image.
  ///
  /// @param[in]  size  The size. See the restrictions about valid sizes above.
  ///
  /// @return     The native buffer config.
  ///
  static NativeBufferConfig MakeForSwapchainImage(const ISize& size);

  constexpr bool operator==(const NativeBufferConfig& o) const {
    return format == o.format && size == o.size && usage == o.usage;
  }

  constexpr bool operator!=(const NativeBufferConfig& o) const {
    return !(*this == o);
  }
};

//------------------------------------------------------------------------------
/// @brief      A wrapper for OH_NativeBuffer
///             https://developer.huawei.com/consumer/en/doc/harmonyos-references-V5/_o_h___native_buffer-V5
///
///             This wrapper creates and owns a handle to a managed native
///             buffer. That is, there is no ability to take a reference to an
///             externally created native buffer.
///
class NativeBuffer {
 public:
  explicit NativeBuffer(NativeBufferConfig config);

  ~NativeBuffer();

  NativeBuffer(const NativeBuffer&) = delete;

  NativeBuffer& operator=(const NativeBuffer&) = delete;

  bool IsValid() const;

  OH_NativeBuffer* GetHandle() const;

  const NativeBufferConfig& GetConfig() const;

  const OH_NativeBuffer_Config& GetOHOSConfig() const;

  static std::optional<OH_NativeBuffer_Config> Config(OH_NativeBuffer* buffer);

  //----------------------------------------------------------------------------
  /// @brief      Get the system wide unique ID of the native buffer if
  ///             possible. This is only available on Android API 31 and above.
  ///             Within the process, the handle are unique.
  ///
  /// @return     The system unique id if one can be obtained.
  ///
  std::optional<uint32_t> GetSystemUniqueID() const;

  //----------------------------------------------------------------------------
  /// @brief      Get the system wide unique ID of the native buffer if
  ///             possible. This is only available on Android API 31 and above.
  ///             Within the process, the handle are unique.
  ///
  /// @return     The system unique id if one can be obtained.
  ///
  static std::optional<uint32_t> GetSystemUniqueID(OH_NativeBuffer* buffer);

  enum class CPUAccessType {
    kRead,
    kWrite,
  };
  //----------------------------------------------------------------------------
  /// @brief      Lock the buffer for CPU access. This call may fail if the
  ///             buffer was not created with one the usages that allow for CPU
  ///             access.
  ///
  /// @param[in]  type  The type
  ///
  /// @return     A host-accessible buffer if there was no error related to
  ///             usage or buffer validity.
  ///
  void* Lock(CPUAccessType type) const;

  //----------------------------------------------------------------------------
  /// @brief      Unlock a mapping previously locked for CPU access.
  ///
  /// @return     If the unlock was successful.
  ///
  bool Unlock() const;

 private:
  struct UniqueOHNativeBufferTraits {
    static OH_NativeBuffer* InvalidValue() { return nullptr; }

    static bool IsValid(OH_NativeBuffer* value) {
      return value != InvalidValue();
    }

    static void Free(OH_NativeBuffer* value) {
      OH_NativeBuffer_Unreference(value);
    }
  };

  const NativeBufferConfig config_;
  const OH_NativeBuffer_Config ohos_config_;
  fml::UniqueObject<OH_NativeBuffer*, UniqueOHNativeBufferTraits> buffer_;
  bool is_valid_ = false;
};

}  // namespace impeller::ohos

namespace impeller {

IMPELLER_ENUM_IS_MASK(ohos::NativeBufferUsageFlags);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_TOOLKIT_OHOS_NATIVE_BUFFER_H_
