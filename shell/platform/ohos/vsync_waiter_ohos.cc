// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/vsync_waiter_ohos.h"

#include "flutter/fml/platform/ohos/napi_util.h"

namespace flutter {

static std::atomic_uint g_refresh_rate_ = 60;

const char* flutterVSync = "flutterVSync";

VsyncWaiterOHOS::VsyncWaiterOHOS(const flutter::TaskRunners& task_runners)
    : VsyncWaiter(task_runners) {
  vsync_ = OH_NativeVSync_Create(flutterVSync, strlen(flutterVSync));
}

VsyncWaiterOHOS::~VsyncWaiterOHOS() {
  OH_NativeVSync_Destroy(vsync_);
  vsync_ = nullptr;
}

void VsyncWaiterOHOS::AwaitVSync() {
  if (vsync_ == nullptr) {
    return;
  }
  auto* weak_this = new std::weak_ptr<VsyncWaiter>(shared_from_this());

  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetUITaskRunner(), [weak_this, vsync = vsync_]() {
        OH_NativeVSync_RequestFrame(vsync, &OnVsyncFromNDK, weak_this);
      });
}

void VsyncWaiterOHOS::OnVsyncFromNDK(long long timestamp, void* data) {
  if (data == nullptr) {
    return;
  }
  int64_t frame_nanos = static_cast<int64_t>(timestamp);
  auto frame_time = fml::TimePoint::FromEpochDelta(
      fml::TimeDelta::FromNanoseconds(frame_nanos));
  auto now = fml::TimePoint::Now();
  if (frame_time > now) {
    frame_time = now;
  }
  auto target_time = frame_time + fml::TimeDelta::FromNanoseconds(
                                      1000000000.0 / g_refresh_rate_);

  TRACE_EVENT2_INT("flutter", "PlatformVsync", "frame_start_time",
                   frame_time.ToEpochDelta().ToMicroseconds(),
                   "frame_target_time",
                   target_time.ToEpochDelta().ToMicroseconds());

  auto* weak_this = reinterpret_cast<std::weak_ptr<VsyncWaiter>*>(data);
  ConsumePendingCallback(weak_this, frame_time, target_time);
}

void VsyncWaiterOHOS::ConsumePendingCallback(
    std::weak_ptr<VsyncWaiter>* weak_this,
    fml::TimePoint frame_start_time,
    fml::TimePoint frame_target_time) {
  auto shared_this = weak_this->lock();
  delete weak_this;

  if (shared_this) {
    shared_this->FireCallback(frame_start_time, frame_target_time);
  }
}

napi_value VsyncWaiterOHOS::OnUpdateRefreshRate(napi_env env,
                                                napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t refresh_rate;
  ret = napi_get_value_int64(env, args[0], &refresh_rate);
  if (ret != napi_ok) {
    return nullptr;
  }

  FML_DCHECK(refresh_rate > 0);
  g_refresh_rate_ = static_cast<int>(refresh_rate);
  return nullptr;
}

bool VsyncWaiterOHOS::Register(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      NAPI_DEFAULT_METHOD("nativeUpdateRefreshRate", OnUpdateRefreshRate),
  };

  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  return true;
}

}  // namespace flutter
