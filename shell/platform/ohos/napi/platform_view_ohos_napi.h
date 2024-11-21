// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_NAPI_PLATFORM_VIEW_OHOS_NAPI_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_NAPI_PLATFORM_VIEW_OHOS_NAPI_H_

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"

#include "flutter/lib/ui/window/platform_message.h"

namespace flutter {

class PlatformViewOHOSNAPI {
 public:
  virtual ~PlatformViewOHOSNAPI();

  //----------------------------------------------------------------------------
  /// @brief      Sends a platform message. The message may be empty.
  ///
  virtual void FlutterViewHandlePlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message,
      int responseId) = 0;

  //----------------------------------------------------------------------------
  /// @brief      Responds to a platform message. The data may be a `nullptr`.
  ///
  virtual void FlutterViewHandlePlatformMessageResponse(
      int responseId,
      std::unique_ptr<fml::Mapping> data) = 0;

  //----------------------------------------------------------------------------
  /// @brief      Indicates that FlutterView should start painting pixels.
  ///
  /// @note       Must be called from the platform thread.
  ///
  virtual void FlutterViewOnFirstFrame() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Indicates that a hot restart is about to happen.
  ///
  virtual void FlutterViewOnPreEngineRestart() = 0;

  virtual std::unique_ptr<std::vector<std::string>>
  FlutterViewComputePlatformResolvedLocale(
      std::vector<std::string> supported_locales_data) = 0;

  virtual double GetDisplayRefreshRate() = 0;

  virtual double GetDisplayWidth() = 0;

  virtual double GetDisplayHeight() = 0;

  virtual double GetDisplayDensity() = 0;

  virtual bool RequestDartDeferredLibrary(int loading_unit_id) = 0;

  virtual double FlutterViewGetScaledFontSize(double unscaled_font_size,
                                              int configuration_id) const = 0;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_OHOS_NAPI_PLATFORM_VIEW_OHOS_NAPI_H_
