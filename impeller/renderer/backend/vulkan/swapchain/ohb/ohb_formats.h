// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_FORMATS_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_FORMATS_H_

#include "impeller/core/formats.h"
#include "impeller/toolkit/ohos/native_buffer.h"

namespace impeller {

constexpr PixelFormat ToPixelFormat(ohos::NativeBufferFormat format) {
  switch (format) {
    case ohos::NativeBufferFormat::kR8G8B8A8UNormInt:
      return PixelFormat::kR8G8B8A8UNormInt;
  }
  FML_UNREACHABLE();
}

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_SWAPCHAIN_OHB_OHB_FORMATS_H_
