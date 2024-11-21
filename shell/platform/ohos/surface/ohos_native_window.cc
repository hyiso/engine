// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/surface/ohos_native_window.h"

namespace flutter {

OHOSNativeWindow::OHOSNativeWindow(Handle window, bool is_fake_window)
    : window_(window), is_fake_window_(is_fake_window) {
  // Set the read and write scenarios of the native window buffer.
  OH_NativeWindow_NativeWindowHandleOpt(window, SET_USAGE,
                                        NATIVEBUFFER_USAGE_MEM_DMA);
  // Set the step of the native window buffer.
  OH_NativeWindow_NativeWindowHandleOpt(window, SET_STRIDE, 0x8);
  // Set the format of the native window buffer.
  OH_NativeWindow_NativeWindowHandleOpt(window, SET_FORMAT,
                                        NATIVEBUFFER_PIXEL_FMT_RGBA_8888);
}

OHOSNativeWindow::OHOSNativeWindow(Handle window)
    : OHOSNativeWindow(window, /*is_fake_window=*/false) {}

OHOSNativeWindow::~OHOSNativeWindow() = default;

bool OHOSNativeWindow::IsValid() const {
  return window_ != nullptr;
}

OHOSNativeWindow::Handle OHOSNativeWindow::handle() const {
  return window_;
}

SkISize OHOSNativeWindow::GetSize() const {
  int32_t width = 0;
  int32_t height = 0;
  if (window_ != nullptr) {
    OH_NativeWindow_NativeWindowHandleOpt(window_, GET_BUFFER_GEOMETRY, &height,
                                          &width);
  }
  return SkISize::Make(width, height);
}

}  // namespace flutter
