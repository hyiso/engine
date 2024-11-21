// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_display.h"

namespace flutter {

OHOSDisplay::OHOSDisplay(std::shared_ptr<PlatformViewOHOSNAPI> napi_facade)
    : Display(0,
              napi_facade->GetDisplayRefreshRate(),
              napi_facade->GetDisplayWidth(),
              napi_facade->GetDisplayHeight(),
              napi_facade->GetDisplayDensity()),
      napi_facade_(std::move(napi_facade)) {}

double OHOSDisplay::GetRefreshRate() const {
  return napi_facade_->GetDisplayRefreshRate();
}

double OHOSDisplay::GetWidth() const {
  return napi_facade_->GetDisplayWidth();
}

double OHOSDisplay::GetHeight() const {
  return napi_facade_->GetDisplayHeight();
}

double OHOSDisplay::GetDevicePixelRatio() const {
  return napi_facade_->GetDisplayDensity();
}

}  // namespace flutter