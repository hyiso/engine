// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_FLUTTER_MAIN_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_FLUTTER_MAIN_H_

#include <napi/native_api.h>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/runtime/dart_service_isolate.h"

namespace flutter {
class FlutterMain {
 public:
  ~FlutterMain();

  static bool Register(napi_env env, napi_value exports);

  static FlutterMain& Get();

  const flutter::Settings& GetSettings() const;

  static OHOSRenderingAPI SelectedRenderingAPI(
      const flutter::Settings& settings);

 private:
  const flutter::Settings settings_;
  DartServiceIsolate::CallbackHandle vm_service_uri_callback_ = 0;

  explicit FlutterMain(const flutter::Settings& settings);

  static napi_value Init(napi_env env, napi_callback_info info);

  void SetupDartVMServiceUriCallback(napi_env env);

  FML_DISALLOW_COPY_AND_ASSIGN(FlutterMain);
};
}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_FLUTTER_MAIN_H_
