// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_PLATFORM_VIEW_OHOS_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_PLATFORM_VIEW_OHOS_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <napi/native_api.h>
#include <native_image/native_image.h>

#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/lib/ui/window/platform_message.h"
#include "flutter/shell/common/platform_view.h"
#include "flutter/shell/platform/ohos/context/ohos_context.h"
#include "flutter/shell/platform/ohos/napi/platform_view_ohos_napi.h"
#include "flutter/shell/platform/ohos/platform_message_handler_ohos.h"
#include "flutter/shell/platform/ohos/surface/ohos_native_window.h"
#include "flutter/shell/platform/ohos/surface/ohos_snapshot_surface_producer.h"
#include "flutter/shell/platform/ohos/surface/ohos_surface.h"
#include "flutter/shell/platform/ohos/surface_texture_external_texture.h"

namespace flutter {

class OHOSSurfaceFactoryImpl : public OHOSSurfaceFactory {
 public:
  OHOSSurfaceFactoryImpl(const std::shared_ptr<OHOSContext>& context,
                         bool enable_impeller);

  ~OHOSSurfaceFactoryImpl() override;

  std::unique_ptr<OHOSSurface> CreateSurface() override;

 private:
  const std::shared_ptr<OHOSContext>& ohos_context_;
  const bool enable_impeller_;
};

class PlatformViewOHOS final : public PlatformView {
 public:
  static bool Register(napi_env env, napi_value exports);

  PlatformViewOHOS(PlatformView::Delegate& delegate,
                   const flutter::TaskRunners& task_runners,
                   const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade,
                   bool use_software_rendering);

  //----------------------------------------------------------------------------
  /// @brief      Creates a new PlatformViewOHOS but using an existing
  ///             OHOS GPU context to create new surfaces. This maximizes
  ///             resource sharing between 2 PlatformViewOHOSs of 2 Shells.
  ///
  PlatformViewOHOS(PlatformView::Delegate& delegate,
                   const flutter::TaskRunners& task_runners,
                   const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade,
                   const std::shared_ptr<flutter::OHOSContext>& ohos_context);

  ~PlatformViewOHOS() override;

  void NotifyCreated(fml::RefPtr<OHOSNativeWindow> native_window);

  void NotifySurfaceWindowChanged(fml::RefPtr<OHOSNativeWindow> native_window);

  void NotifyChanged(const SkISize& size);

  // |PlatformView|
  void NotifyDestroyed() override;

  void DispatchPlatformMessage(std::string name,
                               void* message_data,
                               int message_position,
                               int response_id);

  void DispatchEmptyPlatformMessage(std::string name, int response_id);

  void DispatchSemanticsAction(int id,
                               int action,
                               void* actionData,
                               int actionDataLenth);

  void RegisterExternalTexture(int64_t texture_id,
                               OH_NativeImage* native_image);

  void SetTextureBufferSize(int64_t texture_id, int32_t width, int32_t height);

  void UnRegisterExternalTexture(int64_t texture_id);

  // |PlatformView|
  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override;

  // |PlatformView|
  PointerDataDispatcherMaker GetDispatcherMaker() override;

  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient) override;

  // |PlatformView|
  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override;

  const std::shared_ptr<OHOSContext>& GetOHOSContext() { return ohos_context_; }

  std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler()
      const override {
    return platform_message_handler_;
  }

 private:
  const std::shared_ptr<PlatformViewOHOSNAPI> napi_facade_;
  std::shared_ptr<OHOSContext> ohos_context_;
  std::shared_ptr<OHOSSurfaceFactoryImpl> surface_factory_;

  std::shared_ptr<OHOSSurface> ohos_surface_;
  std::shared_ptr<PlatformMessageHandlerOHOS> platform_message_handler_;

  std::map<int64_t, std::shared_ptr<SurfaceTextureExternalTexture>>
      external_texture_gl_;

  // |PlatformView|
  void UpdateSemantics(
      flutter::SemanticsNodeUpdates update,
      flutter::CustomAccessibilityActionUpdates actions) override;

  // |PlatformView|
  void HandlePlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override;

  // |PlatformView|
  void OnPreEngineRestart() const override;

  // |PlatformView|
  std::unique_ptr<VsyncWaiter> CreateVSyncWaiter() override;

  // |PlatformView|
  std::unique_ptr<Surface> CreateRenderingSurface() override;

  // |PlatformView|
  std::shared_ptr<ExternalViewEmbedder> CreateExternalViewEmbedder() override;

  // |PlatformView|
  std::unique_ptr<SnapshotSurfaceProducer> CreateSnapshotSurfaceProducer()
      override;

  // |PlatformView|
  sk_sp<GrDirectContext> CreateResourceContext() const override;

  // |PlatformView|
  void ReleaseResourceContext() const override;

  // |PlatformView|
  std::shared_ptr<impeller::Context> GetImpellerContext() const override;

  // |PlatformView|
  std::unique_ptr<std::vector<std::string>> ComputePlatformResolvedLocales(
      const std::vector<std::string>& supported_locale_data) override;

  // |PlatformView|
  void RequestDartDeferredLibrary(intptr_t loading_unit_id) override;

  void InstallFirstFrameCallback();

  void FireFirstFrameCallback();

  double GetScaledFontSize(double unscaled_font_size,
                           int configuration_id) const override;

  FML_DISALLOW_COPY_AND_ASSIGN(PlatformViewOHOS);
};

}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_PLATFORM_VIEW_OHOS_H_
