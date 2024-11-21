// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_surface_software.h"

#include <native_buffer/native_buffer.h>
#include <native_window/buffer_handle.h>
#include <sys/mman.h>

#include "flutter/fml/trace_event.h"
#include "third_party/skia/include/core/SkImage.h"

namespace flutter {

bool GetSkColorType(int32_t buffer_format,
                    SkColorType* color_type,
                    SkAlphaType* alpha_type) {
  switch (buffer_format) {
    case NATIVEBUFFER_PIXEL_FMT_RGB_565:
      *color_type = kRGB_565_SkColorType;
      *alpha_type = kOpaque_SkAlphaType;
      return true;
    case NATIVEBUFFER_PIXEL_FMT_RGBA_8888:
      *color_type = kRGBA_8888_SkColorType;
      *alpha_type = kPremul_SkAlphaType;
      return true;
    default:
      return false;
  }
}

OHOSSurfaceSoftware::OHOSSurfaceSoftware() {
  GetSkColorType(NATIVEBUFFER_PIXEL_FMT_RGBA_8888, &target_color_type_,
                 &target_alpha_type_);
}

OHOSSurfaceSoftware::~OHOSSurfaceSoftware() = default;

bool OHOSSurfaceSoftware::IsValid() const {
  return true;
}

bool OHOSSurfaceSoftware::ResourceContextMakeCurrent() {
  // Resource Context always not available on software backend.
  return false;
}

bool OHOSSurfaceSoftware::ResourceContextClearCurrent() {
  return false;
}

std::unique_ptr<Surface> OHOSSurfaceSoftware::CreateGPUSurface(
    // The software OHOSSurface neither uses any passed in Skia context
    // nor does it interact with the OHOSContext's raster Skia context.
    GrDirectContext* gr_context) {
  if (!IsValid()) {
    return nullptr;
  }

  auto surface =
      std::make_unique<GPUSurfaceSoftware>(this, true /* render to surface */);

  if (!surface->IsValid()) {
    return nullptr;
  }
  return surface;
}

sk_sp<SkSurface> OHOSSurfaceSoftware::AcquireBackingStore(const SkISize& size) {
  TRACE_EVENT0("flutter", "OHOSSurfaceSoftware::AcquireBackingStore");
  if (!IsValid()) {
    return nullptr;
  }

  if (sk_surface_ != nullptr &&
      SkISize::Make(sk_surface_->width(), sk_surface_->height()) == size) {
    // The old and new surface sizes are the same. Nothing to do here.
    return sk_surface_;
  }

  SkImageInfo image_info =
      SkImageInfo::Make(size.fWidth, size.fHeight, target_color_type_,
                        target_alpha_type_, SkColorSpace::MakeSRGB());

  sk_surface_ = SkSurfaces::Raster(image_info);

  return sk_surface_;
}

bool OHOSSurfaceSoftware::PresentBackingStore(sk_sp<SkSurface> backing_store) {
  TRACE_EVENT0("flutter", "OHOSSurfaceSoftware::PresentBackingStore");
  if (!IsValid() || backing_store == nullptr) {
    return false;
  }

  SkPixmap pixmap;
  if (!backing_store->peekPixels(&pixmap)) {
    return false;
  }

  OHNativeWindowBuffer* buffer = nullptr;
  int releaseFenceFd = -1;
  int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(
      native_window_->handle(), &buffer, &releaseFenceFd);
  if (ret != 0) {
    return false;
  }

  BufferHandle* handle = OH_NativeWindow_GetBufferHandleFromNative(buffer);

  if (handle == nullptr) {
    OH_NativeWindow_DestroyNativeWindowBuffer(buffer);
    return false;
  }
  void* bits = mmap(nullptr, handle->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    handle->fd, 0);
  if (bits == MAP_FAILED) {
    OH_NativeWindow_DestroyNativeWindowBuffer(buffer);
    return false;
  }

  SkColorType color_type;
  SkAlphaType alpha_type;
  if (GetSkColorType(handle->format, &color_type, &alpha_type)) {
    SkImageInfo native_image_info = SkImageInfo::Make(
        handle->width, handle->height, color_type, alpha_type);

    std::unique_ptr<SkCanvas> canvas = SkCanvas::MakeRasterDirect(
        native_image_info, bits,
        handle->stride * SkColorTypeBytesPerPixel(color_type));

    if (canvas) {
      SkBitmap bitmap;
      if (bitmap.installPixels(pixmap)) {
        canvas->drawImageRect(bitmap.asImage(),
                              SkRect::MakeIWH(handle->width, handle->height),
                              SkSamplingOptions());
      }
    }
  }

  Region region{nullptr, 0};
  int acquireFenceFd = -1;
  ret = OH_NativeWindow_NativeWindowFlushBuffer(native_window_->handle(),
                                                buffer, acquireFenceFd, region);
  OH_NativeWindow_DestroyNativeWindowBuffer(buffer);
  if (bits != nullptr) {
    munmap(bits, handle->size);
  }
  return ret == 0;
}

void OHOSSurfaceSoftware::TeardownOnScreenContext() {}

bool OHOSSurfaceSoftware::OnScreenSurfaceResize(const SkISize& size) {
  return true;
}

bool OHOSSurfaceSoftware::SetNativeWindow(
    fml::RefPtr<OHOSNativeWindow> window) {
  native_window_ = std::move(window);
  if (!(native_window_ && native_window_->IsValid())) {
    return false;
  }
  int32_t window_format = -1;
  int ret = OH_NativeWindow_NativeWindowHandleOpt(native_window_->handle(),
                                                  GET_FORMAT, &window_format);
  if (ret != 0 || window_format < 0) {
    return false;
  }
  if (!GetSkColorType(window_format, &target_color_type_,
                      &target_alpha_type_)) {
    return false;
  }
  return true;
}

}  // namespace flutter
