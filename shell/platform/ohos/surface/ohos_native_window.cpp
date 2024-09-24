/*
 * Copyright (c) 2023 Hunan OpenValley Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "flutter/shell/platform/ohos/surface/ohos_native_window.h"
#include "flutter/fml/logging.h"

namespace flutter {

OHOSNativeWindow::OHOSNativeWindow(Handle window)
    : window_(window), is_fake_window_(false) {
  FML_LOG(INFO) << " native_window:" << (int64_t)window_;
}

OHOSNativeWindow::~OHOSNativeWindow() {
  if (window_ != nullptr) {
    window_ = nullptr;
  }
}

bool OHOSNativeWindow::IsValid() const {
  return window_ != nullptr;
}

SkISize OHOSNativeWindow::GetSize() const {
  int width = 0;
  int height = 0;
  if (window_ != nullptr) {
    OH_NativeWindow_NativeWindowHandleOpt(window_, GET_BUFFER_GEOMETRY, &height, &width);
  }
  return SkISize::Make(width, height);
}

OHOSNativeWindow::Handle OHOSNativeWindow::handle() const {
  return window_;
}

}  // namespace flutter
