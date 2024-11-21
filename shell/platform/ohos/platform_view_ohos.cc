// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/platform_view_ohos.h"

#include "flutter/fml/make_copyable.h"
#include "flutter/shell/common/shell_io_manager.h"
#include "flutter/shell/platform/ohos/ohos_context_gl_impeller.h"
#include "flutter/shell/platform/ohos/ohos_context_gl_skia.h"
#include "flutter/shell/platform/ohos/ohos_context_vulkan_impeller.h"
#include "flutter/shell/platform/ohos/ohos_surface_gl_impeller.h"
#include "flutter/shell/platform/ohos/ohos_surface_gl_skia.h"
#include "flutter/shell/platform/ohos/ohos_surface_software.h"
#if IMPELLER_ENABLE_VULKAN  // b/258506856 for why this is behind an if
#include "flutter/shell/platform/ohos/ohos_surface_vulkan_impeller.h"
#endif
#include "flutter/shell/platform/ohos/platform_message_response_ohos.h"
#include "flutter/shell/platform/ohos/surface_texture_external_texture_gl.h"
#include "flutter/shell/platform/ohos/vsync_waiter_ohos.h"

#include <GLES2/gl2ext.h>

namespace flutter {

OHOSSurfaceFactoryImpl::OHOSSurfaceFactoryImpl(
    const std::shared_ptr<OHOSContext>& context,
    bool enable_impeller)
    : ohos_context_(context), enable_impeller_(enable_impeller) {}

OHOSSurfaceFactoryImpl::~OHOSSurfaceFactoryImpl() = default;

std::unique_ptr<OHOSSurface> OHOSSurfaceFactoryImpl::CreateSurface() {
  switch (ohos_context_->RenderingApi()) {
    case OHOSRenderingAPI::kSoftware:
      return std::make_unique<OHOSSurfaceSoftware>();
    case OHOSRenderingAPI::kImpellerOpenGLES:
      if (!enable_impeller_) {
        FML_LOG(ERROR) << "Impeller is not enabled.";
      }
      return std::make_unique<OHOSSurfaceGLImpeller>(
          std::static_pointer_cast<OHOSContextGLImpeller>(ohos_context_));
    case OHOSRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<OHOSSurfaceGLSkia>(
          std::static_pointer_cast<OHOSContextGLSkia>(ohos_context_));
    case OHOSRenderingAPI::kImpellerVulkan:
      return std::make_unique<OHOSSurfaceVulkanImpeller>(
          std::static_pointer_cast<OHOSContextVulkanImpeller>(ohos_context_));
  }
  FML_UNREACHABLE();
}

std::shared_ptr<OHOSContext> CreateOHOSContext(
    bool use_software_rendering,
    const flutter::TaskRunners& task_runners,
    OHOSRenderingAPI ohos_rendering_api,
    bool enable_vulkan_validation,
    bool enable_opengl_gpu_tracing,
    bool enable_vulkan_gpu_tracing) {
  switch (ohos_rendering_api) {
    case OHOSRenderingAPI::kSoftware:
      return std::make_shared<OHOSContext>(OHOSRenderingAPI::kSoftware);
    case OHOSRenderingAPI::kImpellerOpenGLES:
      return std::make_unique<OHOSContextGLImpeller>(
          std::make_unique<impeller::egl::Display>(),
          enable_opengl_gpu_tracing);
    case OHOSRenderingAPI::kImpellerVulkan:
      return std::make_unique<OHOSContextVulkanImpeller>(
          enable_vulkan_validation, enable_vulkan_gpu_tracing);
    case OHOSRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<OHOSContextGLSkia>(
          fml::MakeRefCounted<OHOSEnvironmentGL>(),  //
          task_runners                               //
      );
  }
  FML_UNREACHABLE();
}

PlatformViewOHOS::PlatformViewOHOS(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade,
    bool use_software_rendering)
    : PlatformViewOHOS(
          delegate,
          task_runners,
          napi_facade,
          CreateOHOSContext(
              use_software_rendering,
              task_runners,
              delegate.OnPlatformViewGetSettings().ohos_rendering_api,
              delegate.OnPlatformViewGetSettings().enable_vulkan_validation,
              delegate.OnPlatformViewGetSettings().enable_opengl_gpu_tracing,
              delegate.OnPlatformViewGetSettings().enable_vulkan_gpu_tracing)) {
}

PlatformViewOHOS::PlatformViewOHOS(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade,
    const std::shared_ptr<flutter::OHOSContext>& ohos_context)
    : PlatformView(delegate, task_runners),
      napi_facade_(napi_facade),
      ohos_context_(ohos_context),
      platform_message_handler_(new PlatformMessageHandlerOHOS(
          napi_facade,
          task_runners.GetPlatformTaskRunner())) {
  if (ohos_context_) {
    FML_CHECK(ohos_context_->IsValid())
        << "Could not create surface from invalid OHOS context.";
    surface_factory_ = std::make_shared<OHOSSurfaceFactoryImpl>(
        ohos_context_,                                        //
        delegate.OnPlatformViewGetSettings().enable_impeller  //
    );
    ohos_surface_ = surface_factory_->CreateSurface();
    FML_CHECK(ohos_surface_ && ohos_surface_->IsValid())
        << "Could not create an OpenGL, Vulkan or Software surface to set up "
           "rendering.";
  }
}

PlatformViewOHOS::~PlatformViewOHOS() = default;

void PlatformViewOHOS::NotifyCreated(
    fml::RefPtr<OHOSNativeWindow> native_window) {
  if (ohos_surface_) {
    InstallFirstFrameCallback();

    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetRasterTaskRunner(),
        [&latch, surface = ohos_surface_.get(),
         native_window = std::move(native_window)]() {
          surface->SetNativeWindow(native_window);
          latch.Signal();
        });
    latch.Wait();
  }

  PlatformView::NotifyCreated();
}

void PlatformViewOHOS::NotifySurfaceWindowChanged(
    fml::RefPtr<OHOSNativeWindow> native_window) {
  if (ohos_surface_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetRasterTaskRunner(),
        [&latch, surface = ohos_surface_.get(),
         native_window = std::move(native_window)]() {
          surface->TeardownOnScreenContext();
          surface->SetNativeWindow(native_window);
          latch.Signal();
        });
    latch.Wait();
  }

  PlatformView::ScheduleFrame();
}

void PlatformViewOHOS::NotifyDestroyed() {
  PlatformView::NotifyDestroyed();

  if (ohos_surface_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        task_runners_.GetRasterTaskRunner(),
        [&latch, surface = ohos_surface_.get()]() {
          surface->TeardownOnScreenContext();
          latch.Signal();
        });
    latch.Wait();
  }
}

void PlatformViewOHOS::NotifyChanged(const SkISize& size) {
  if (!ohos_surface_) {
    return;
  }
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetRasterTaskRunner(),  //
      [&latch, surface = ohos_surface_.get(), size]() {
        surface->OnScreenSurfaceResize(size);
        latch.Signal();
      });
  latch.Wait();
}

void PlatformViewOHOS::DispatchPlatformMessage(std::string name,
                                               void* message_data,
                                               int message_position,
                                               int response_id) {
  fml::MallocMapping message =
      fml::MallocMapping::Copy(message_data, message_position);

  fml::RefPtr<flutter::PlatformMessageResponse> response;
  response = fml::MakeRefCounted<PlatformMessageResponseOHOS>(
      response_id, napi_facade_, task_runners_.GetPlatformTaskRunner());

  PlatformView::DispatchPlatformMessage(
      std::make_unique<flutter::PlatformMessage>(
          std::move(name), std::move(message), std::move(response)));
}

void PlatformViewOHOS::DispatchEmptyPlatformMessage(std::string name,
                                                    int response_id) {
  fml::RefPtr<flutter::PlatformMessageResponse> response;
  response = fml::MakeRefCounted<PlatformMessageResponseOHOS>(
      response_id, napi_facade_, task_runners_.GetPlatformTaskRunner());

  PlatformView::DispatchPlatformMessage(
      std::make_unique<flutter::PlatformMessage>(std::move(name),
                                                 std::move(response)));
}

void PlatformViewOHOS::DispatchSemanticsAction(int id,
                                               int action,
                                               void* actionData,
                                               int actionDataLenth) {
  auto args_vector = fml::MallocMapping::Copy(actionData, actionDataLenth);

  PlatformView::DispatchSemanticsAction(
      id, static_cast<flutter::SemanticsAction>(action),
      std::move(args_vector));
}

// |PlatformView|
void PlatformViewOHOS::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                    std::move(snapshot_instructions));
}

void PlatformViewOHOS::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                         transient);
}

// |PlatformView|
void PlatformViewOHOS::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  delegate_.UpdateAssetResolverByType(std::move(updated_asset_resolver), type);
}

// |PlatformView|
void PlatformViewOHOS::UpdateSemantics(
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  // TODO: Implement
}

// |PlatformView|
void PlatformViewOHOS::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  platform_message_handler_->HandlePlatformMessage(std::move(message));
}

// |PlatformView|
void PlatformViewOHOS::OnPreEngineRestart() const {
  task_runners_.GetPlatformTaskRunner()->PostTask(
      fml::MakeCopyable([napi_facede = napi_facade_]() mutable {
        napi_facede->FlutterViewOnPreEngineRestart();
      }));
}

// |PlatformView|
std::unique_ptr<VsyncWaiter> PlatformViewOHOS::CreateVSyncWaiter() {
  return std::make_unique<VsyncWaiterOHOS>(task_runners_);
}

// |PlatformView|
std::unique_ptr<Surface> PlatformViewOHOS::CreateRenderingSurface() {
  if (ohos_surface_ == nullptr) {
    return nullptr;
  }
  return ohos_surface_->CreateGPUSurface(
      ohos_context_->GetMainSkiaContext().get());
}

// |PlatformView|
std::shared_ptr<ExternalViewEmbedder>
PlatformViewOHOS::CreateExternalViewEmbedder() {
  return nullptr;
}

// |PlatformView|
std::unique_ptr<SnapshotSurfaceProducer>
PlatformViewOHOS::CreateSnapshotSurfaceProducer() {
  return std::make_unique<OHOSSnapshotSurfaceProducer>(*ohos_surface_);
}

// |PlatformView|
sk_sp<GrDirectContext> PlatformViewOHOS::CreateResourceContext() const {
  if (!ohos_surface_) {
    return nullptr;
  }
  sk_sp<GrDirectContext> resource_context;
  if (ohos_surface_->ResourceContextMakeCurrent()) {
    // TODO(chinmaygarde): Currently, this code depends on the fact that only
    // the OpenGL surface will be able to make a resource context current. If
    // this changes, this assumption breaks. Handle the same.
    resource_context = ShellIOManager::CreateCompatibleResourceLoadingContext(
        GrBackend::kOpenGL,
        GPUSurfaceGLDelegate::GetDefaultPlatformGLInterface());
  } else {
    FML_DLOG(ERROR) << "Could not make the resource context current.";
  }

  return resource_context;
}

// |PlatformView|
void PlatformViewOHOS::ReleaseResourceContext() const {
  if (ohos_surface_) {
    ohos_surface_->ResourceContextClearCurrent();
  }
}

// |PlatformView|
std::shared_ptr<impeller::Context> PlatformViewOHOS::GetImpellerContext()
    const {
  if (ohos_surface_) {
    return ohos_surface_->GetImpellerContext();
  }
  return nullptr;
}

// |PlatformView|
std::unique_ptr<std::vector<std::string>>
PlatformViewOHOS::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  return napi_facade_->FlutterViewComputePlatformResolvedLocale(
      supported_locale_data);
}

// |PlatformView|
void PlatformViewOHOS::RequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (napi_facade_->RequestDartDeferredLibrary(loading_unit_id)) {
    return;
  }
  return;  // TODO(garyq): Call LoadDartDeferredLibraryFailure()
}

void PlatformViewOHOS::InstallFirstFrameCallback() {
  SetNextFrameCallback(
      [platform_view = GetWeakPtr(),
       platform_task_runner = task_runners_.GetPlatformTaskRunner()]() {
        platform_task_runner->PostTask([platform_view]() {
          // Back on Platform Task Runner.
          if (platform_view) {
            reinterpret_cast<PlatformViewOHOS*>(platform_view.get())
                ->FireFirstFrameCallback();
          }
        });
      });
}

void PlatformViewOHOS::FireFirstFrameCallback() {
  napi_facade_->FlutterViewOnFirstFrame();
}

double PlatformViewOHOS::GetScaledFontSize(double unscaled_font_size,
                                           int configuration_id) const {
  return napi_facade_->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                    configuration_id);
}

PointerDataDispatcherMaker PlatformViewOHOS::GetDispatcherMaker() {
  return [](DefaultPointerDataDispatcher::Delegate& delegate) {
    return std::make_unique<DefaultPointerDataDispatcher>(delegate);
  };
}

void PlatformViewOHOS::RegisterExternalTexture(int64_t texture_id,
                                               OH_NativeImage* native_image) {
  std::shared_ptr<SurfaceTextureExternalTexture> external_texture;
  switch (ohos_context_->RenderingApi()) {
    case OHOSRenderingAPI::kImpellerOpenGLES:
      // Impeller GLES.
      external_texture =
          std::make_shared<SurfaceTextureExternalTextureImpellerGL>(
              std::static_pointer_cast<impeller::ContextGLES>(
                  ohos_context_->GetImpellerContext()),
              texture_id, native_image, napi_facade_);
      break;
    case OHOSRenderingAPI::kSkiaOpenGLES:
      // Legacy GL.
      external_texture = std::make_shared<SurfaceTextureExternalTextureGL>(
          texture_id, native_image, napi_facade_);
      break;
    case OHOSRenderingAPI::kSoftware:
      FML_LOG(INFO) << "Software rendering does not support external textures.";
      break;
    case OHOSRenderingAPI::kImpellerVulkan:
      FML_LOG(ERROR) << "Impeller requires migrating plugins that create and "
                        "register surface textures to the new surface producer "
                        "API. See "
                        "https://docs.flutter.dev/release/breaking-changes/"
                        "android-surface-plugins";
  }
  RegisterTexture(external_texture);
  external_texture_gl_[texture_id] = external_texture;
}

void PlatformViewOHOS::SetTextureBufferSize(int64_t texture_id,
                                            int32_t width,
                                            int32_t height) {
  auto iter = external_texture_gl_.find(texture_id);
  if (iter != external_texture_gl_.end()) {
    iter->second->setTextureBufferSize(width, height);
  }
}

void PlatformViewOHOS::UnRegisterExternalTexture(int64_t texture_id) {
  external_texture_gl_.erase(texture_id);
  UnregisterTexture(texture_id);
}

}  // namespace flutter
