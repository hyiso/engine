// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/surface/ohos_surface.h"

namespace flutter {

OHOSSurface::OHOSSurface() = default;

OHOSSurface::~OHOSSurface() = default;

std::unique_ptr<Surface> OHOSSurface::CreateSnapshotSurface() {
  return nullptr;
}

std::shared_ptr<impeller::Context> OHOSSurface::GetImpellerContext() {
  return nullptr;
}

}  // namespace flutter