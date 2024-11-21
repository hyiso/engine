// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_PLATFORM_VIEW_OHOS_NAPI_IMPL_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_PLATFORM_VIEW_OHOS_NAPI_IMPL_H_

#include <napi/native_api.h>
#include <native_image/native_image.h>

#include "flutter/shell/platform/ohos/napi/platform_view_ohos_napi.h"

namespace flutter {

struct locale {
  std::string language;
  std::string script;
  std::string region;
};

class SurfaceTexture {
 public:
  explicit SurfaceTexture(int64_t tex_name);
  ~SurfaceTexture();

  OH_NativeImage* native_image_;
  OHNativeWindow* native_window_;
};

//------------------------------------------------------------------------------
/// @brief      Concrete implementation of `PlatformViewOHOSNAPI` that is
///             compiled with the Ohos toolchain.
///
class PlatformViewOHOSNAPIImpl final : public PlatformViewOHOSNAPI {
 public:
  explicit PlatformViewOHOSNAPIImpl(napi_env env, napi_value obj);

  ~PlatformViewOHOSNAPIImpl() override;

  void FlutterViewHandlePlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message,
      int responseId) override;

  void FlutterViewHandlePlatformMessageResponse(
      int responseId,
      std::unique_ptr<fml::Mapping> data) override;

  void FlutterViewOnFirstFrame() override;

  void FlutterViewOnPreEngineRestart() override;

  std::unique_ptr<std::vector<std::string>>
  FlutterViewComputePlatformResolvedLocale(
      std::vector<std::string> supported_locales_data) override;

  double GetDisplayRefreshRate() override;

  double GetDisplayWidth() override;

  double GetDisplayHeight() override;

  double GetDisplayDensity() override;

  bool RequestDartDeferredLibrary(int loading_unit_id) override;

  double FlutterViewGetScaledFontSize(double unscaled_font_size,
                                      int configuration_id) const override;

 private:
  napi_env env_;
  napi_ref ref_;

  FML_DISALLOW_COPY_AND_ASSIGN(PlatformViewOHOSNAPIImpl);
};

}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_PLATFORM_VIEW_OHOS_NAPI_IMPL_H_
