// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/surface/ohos_snapshot_surface_producer.h"

namespace flutter {

OHOSSnapshotSurfaceProducer::OHOSSnapshotSurfaceProducer(
    OHOSSurface& ohos_surface)
    : ohos_surface_(ohos_surface) {}

// |SnapshotSurfaceProducer|
std::unique_ptr<Surface> OHOSSnapshotSurfaceProducer::CreateSnapshotSurface() {
  return ohos_surface_.CreateSnapshotSurface();
}

}  // namespace flutter
