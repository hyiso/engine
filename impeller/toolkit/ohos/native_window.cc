// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/toolkit/ohos/native_window.h"

namespace impeller::ohos {

NativeWindow::NativeWindow(OHNativeWindow* window) : window_(window) {}

NativeWindow::~NativeWindow() = default;

bool NativeWindow::IsValid() const {
  return window_.is_valid();
}

ISize NativeWindow::GetSize() const {
  if (!IsValid()) {
    return {};
  }
  int32_t width = 0;
  int32_t height = 0;
  OH_NativeWindow_NativeWindowHandleOpt(window_.get(), GET_BUFFER_GEOMETRY,
                                        &height, &width);
  return ISize::MakeWH(width, height);
}

OHNativeWindow* NativeWindow::GetHandle() const {
  return window_.get();
}

}  // namespace impeller::ohos
