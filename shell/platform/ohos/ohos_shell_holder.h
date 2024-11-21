// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SHELL_HOLDER_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SHELL_HOLDER_H_

#include "flutter/assets/asset_manager.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/unique_fd.h"
#include "flutter/lib/ui/window/viewport_metrics.h"
#include "flutter/runtime/platform_data.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/ohos/napi/platform_view_ohos_napi.h"
#include "flutter/shell/platform/ohos/ohos_asset_provider.h"
#include "flutter/shell/platform/ohos/platform_view_ohos.h"

namespace flutter {

//----------------------------------------------------------------------------
/// @brief      This is the OHOS owner of the core engine Shell.
///
/// @details    This is the top orchestrator class on the C++ side for the
///             OHOS embedding. It corresponds to a FlutterEngine on the
///             ArkTS side. This class is in C++ because the Shell is in
///             C++ and an OHOS orchestrator needs to exist to
///             compose it with other OHOS specific C++ components such as
///             the PlatformViewOHOS. This composition of many-to-one
///             C++ components would be difficult to do through NAPI whereas
///             a FlutterEngine and OHOSShellHolder has a 1:1 relationship.
///
///             Technically, the FlutterNAPI class owns this OHOSShellHolder
///             class instance, but the FlutterNAPI class is meant to be mostly
///             static and has minimal state to perform the C++ pointer <->
///             ArkTS class instance translation.
///
class OHOSShellHolder {
 public:
  OHOSShellHolder(const flutter::Settings& settings,
                  std::shared_ptr<PlatformViewOHOSNAPI> napi_facade);

  ~OHOSShellHolder();

  bool IsValid() const;

  //----------------------------------------------------------------------------
  /// @brief      This is a factory for a derived OHOSShellHolder from an
  ///             existing OHOSShellHolder.
  ///
  /// @details    Creates one Shell from another Shell where the created
  ///             Shell takes the opportunity to share any internal components
  ///             it can. This results is a Shell that has a smaller startup
  ///             time cost and a smaller memory footprint than an Shell created
  ///             with a Create function.
  ///
  ///             The new Shell is returned in a new OHOSShellHolder
  ///             instance.
  ///
  ///             The new Shell's flutter::Settings cannot be changed from that
  ///             of the initial Shell. The RunConfiguration subcomponent can
  ///             be changed however in the spawned Shell to run a different
  ///             entrypoint than the existing shell.
  ///
  ///             Since the OHOSShellHolder both binds downwards to a Shell
  ///             and also upwards to NAPI callbacks that the PlatformViewOHOS
  ///             makes, the NAPI instance holding this OHOSShellHolder should
  ///             be created first to supply the napi_facade callback.
  ///
  /// @param[in]  napi_facade this argument should be the NAPI callback facade
  /// of
  ///             a new NAPI instance meant to hold this OHOSShellHolder.
  ///
  /// @returns    A new OHOSShellHolder containing a new Shell. Returns
  ///             nullptr when a new Shell can't be created.
  ///
  std::unique_ptr<OHOSShellHolder> Spawn(
      std::shared_ptr<PlatformViewOHOSNAPI> napi_facade,
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args) const;

  void Launch(std::unique_ptr<OHOSAssetProvider> apk_asset_provider,
              const std::string& entrypoint,
              const std::string& libraryUrl,
              const std::vector<std::string>& entrypoint_args);

  const flutter::Settings& GetSettings() const;

  fml::WeakPtr<PlatformViewOHOS> GetPlatformView();

  void NotifyLowMemoryWarning();

  const std::shared_ptr<PlatformMessageHandler>& GetPlatformMessageHandler()
      const {
    return shell_->GetPlatformMessageHandler();
  }

  void UpdateDisplayMetrics();

 private:
  const flutter::Settings settings_;
  const std::shared_ptr<PlatformViewOHOSNAPI> napi_facade_;
  fml::WeakPtr<PlatformViewOHOS> platform_view_;
  std::shared_ptr<ThreadHost> thread_host_;
  std::unique_ptr<Shell> shell_;
  uint64_t next_pointer_flow_id_ = 0;
  std::unique_ptr<OHOSAssetProvider> asset_provider_;

  //----------------------------------------------------------------------------
  /// @brief      Constructor with its components injected.
  ///
  /// @details    This is similar to the standard constructor, except its
  ///             members were constructed elsewhere and injected.
  ///
  ///             All injected components must be non-null and valid.
  ///
  ///             Used when constructing the Shell from the inside out when
  ///             spawning from an existing Shell.
  ///
  OHOSShellHolder(const flutter::Settings& settings,
                  const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade,
                  const std::shared_ptr<ThreadHost>& thread_host,
                  std::unique_ptr<Shell> shell,
                  std::unique_ptr<OHOSAssetProvider> apk_asset_provider,
                  const fml::WeakPtr<PlatformViewOHOS>& platform_view);

  static void ThreadDestructCallback(void* value);
  std::optional<RunConfiguration> BuildRunConfiguration(
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::vector<std::string>& entrypoint_args) const;

  FML_DISALLOW_COPY_AND_ASSIGN(OHOSShellHolder);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_OHOS_OHOS_SHELL_HOLDER_H_
