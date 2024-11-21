// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <napi/native_api.h>

#include "flutter/fml/platform/ohos/napi_util.h"
#include "flutter/shell/platform/ohos/flutter_main.h"
#include "flutter/shell/platform/ohos/platform_view_ohos.h"
#include "flutter/shell/platform/ohos/vsync_waiter_ohos.h"

EXTERN_C_START
static napi_value Register(napi_env env, napi_value exports) {
  bool result = false;

  fml::napi::InitEnv(env);

  // Register FlutterMain.
  result = flutter::FlutterMain::Register(env, exports);
  FML_CHECK(result);

  // Register PlatformView
  result = flutter::PlatformViewOHOS::Register(env, exports);
  FML_CHECK(result);

  // Register VSyncWaiter.
  result = flutter::VsyncWaiterOHOS::Register(env, exports);
  FML_CHECK(result);

  return exports;
}
EXTERN_C_END

static napi_module flutterModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Register,
    .nm_modname = "flutter",
    .nm_priv = nullptr,
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterFlutterModule(void) {
  napi_module_register(&flutterModule);
}
