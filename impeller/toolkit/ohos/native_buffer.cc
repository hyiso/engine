// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/toolkit/ohos/native_buffer.h"

#include "impeller/base/validation.h"

namespace impeller::ohos {

static OH_NativeBuffer_Format ToNativeBufferFormat(NativeBufferFormat format) {
  switch (format) {
    case NativeBufferFormat::kR8G8B8A8UNormInt:
      return NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
  }
  FML_UNREACHABLE();
}

static OH_NativeBuffer_Config ToNativeBufferConfig(
    const NativeBufferConfig& config) {
  OH_NativeBuffer_Config oh_config = {};
  oh_config.width = config.size.width;
  oh_config.height = config.size.height;
  oh_config.format = ToNativeBufferFormat(config.format);
  if (config.usage & NativeBufferUsageFlags::kCPURead) {
    oh_config.usage |= NATIVEBUFFER_USAGE_CPU_READ;
  }
  if (config.usage & NativeBufferUsageFlags::kCPUWrite) {
    oh_config.usage |= NATIVEBUFFER_USAGE_CPU_WRITE;
  }
  if (config.usage & NativeBufferUsageFlags::kMemDma) {
    oh_config.usage |= NATIVEBUFFER_USAGE_MEM_DMA;
  }
  if (config.usage & NativeBufferUsageFlags::kHWRender) {
    oh_config.usage |= NATIVEBUFFER_USAGE_HW_RENDER;
  }
  if (config.usage & NativeBufferUsageFlags::kHWTexture) {
    oh_config.usage |= NATIVEBUFFER_USAGE_HW_TEXTURE;
  }
  if (config.usage & NativeBufferUsageFlags::kCPUReadOften) {
    oh_config.usage |= NATIVEBUFFER_USAGE_CPU_READ_OFTEN;
  }
  if (config.usage & NativeBufferUsageFlags::kAlignment512) {
    oh_config.usage |= NATIVEBUFFER_USAGE_ALIGNMENT_512;
  }
  return oh_config;
}

NativeBuffer::NativeBuffer(NativeBufferConfig config)
    : config_(config), ohos_config_(ToNativeBufferConfig(config_)) {
  OH_NativeBuffer* buffer = OH_NativeBuffer_Alloc(&ohos_config_);
  if (buffer == nullptr) {
    VALIDATION_LOG << "Could not alloc native buffer.";
    return;
  }
  buffer_.reset(buffer);
  is_valid_ = true;
}

NativeBuffer::~NativeBuffer() = default;

bool NativeBuffer::IsValid() const {
  return is_valid_;
}

OH_NativeBuffer* NativeBuffer::GetHandle() const {
  return buffer_.get();
}

NativeBufferConfig NativeBufferConfig::MakeForSwapchainImage(
    const ISize& size) {
  NativeBufferConfig config;
  config.format = NativeBufferFormat::kR8G8B8A8UNormInt;
  // Zero sized hardware buffers cannot be allocated.
  config.size = size.Max(ISize{1u, 1u});
  config.usage = NativeBufferUsageFlags::kCPURead |
                 NativeBufferUsageFlags::kCPUWrite |
                 NativeBufferUsageFlags::kMemDma;
  return config;
}

const NativeBufferConfig& NativeBuffer::GetConfig() const {
  return config_;
}

const OH_NativeBuffer_Config& NativeBuffer::GetOHOSConfig() const {
  return ohos_config_;
}

std::optional<uint32_t> NativeBuffer::GetSystemUniqueID() const {
  return GetSystemUniqueID(GetHandle());
}

std::optional<uint32_t> NativeBuffer::GetSystemUniqueID(
    OH_NativeBuffer* buffer) {
  return OH_NativeBuffer_GetSeqNum(buffer);
}

std::optional<OH_NativeBuffer_Config> NativeBuffer::Config(
    OH_NativeBuffer* buffer) {
  OH_NativeBuffer_Config config = {};
  OH_NativeBuffer_GetConfig(buffer, &config);
  return config;
}

void* NativeBuffer::Lock(CPUAccessType type) const {
  if (!is_valid_) {
    return nullptr;
  }
  uint64_t usage = 0;
  switch (type) {
    case CPUAccessType::kRead:
      usage |= NATIVEBUFFER_USAGE_CPU_READ;
      break;
    case CPUAccessType::kWrite:
      usage |= NATIVEBUFFER_USAGE_CPU_WRITE;
      break;
  }
  void* buffer = nullptr;
  int32_t ret = OH_NativeBuffer_Map(buffer_.get(), &buffer);
  return ret == 0 ? buffer : nullptr;
}

bool NativeBuffer::Unlock() const {
  if (!is_valid_) {
    return false;
  }
  int32_t ret = OH_NativeBuffer_Unmap(buffer_.get());
  ;
  return ret == 0;
}

}  // namespace impeller::ohos
