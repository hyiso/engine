// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_VSYNC_WAITER_OHOS_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_VSYNC_WAITER_OHOS_H_
#include <napi/native_api.h>
#include <native_vsync/native_vsync.h>
#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/shell/common/vsync_waiter.h"

namespace flutter {

class VsyncWaiterOHOS final : public VsyncWaiter {
 public:
  static bool Register(napi_env env, napi_value exports);

  explicit VsyncWaiterOHOS(const flutter::TaskRunners& task_runners);

  ~VsyncWaiterOHOS() override;

 private:
  // |VsyncWaiter|
  void AwaitVSync() override;

  static void OnVsyncFromNDK(long long timestamp, void* data);

  static void ConsumePendingCallback(std::weak_ptr<VsyncWaiter>* weak_this,
                                     fml::TimePoint frame_start_time,
                                     fml::TimePoint frame_target_time);

  static napi_value OnUpdateRefreshRate(napi_env env, napi_callback_info info);

  OH_NativeVSync* vsync_;
  FML_DISALLOW_COPY_AND_ASSIGN(VsyncWaiterOHOS);
};
}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_VSYNC_WAITER_OHOS_H_
