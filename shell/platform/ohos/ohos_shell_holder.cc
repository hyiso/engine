// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/ohos/ohos_shell_holder.h"

#include <qos/qos.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "flutter/fml/native_library.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/ohos/ohos_display.h"
#include "flutter/shell/platform/ohos/ohos_image_generator.h"

namespace flutter {

static void OHOSPlatformThreadConfigSetter(
    const fml::Thread::ThreadConfig& config) {
  fml::Thread::SetCurrentThreadName(config);
  // set thread priority
  switch (config.priority) {
    case fml::Thread::ThreadPriority::kBackground: {
      OH_QoS_SetThreadQoS(QoS_Level::QOS_BACKGROUND);
      if (::setpriority(PRIO_PROCESS, 0, 10) != 0) {
        FML_LOG(ERROR) << "Failed to set IO task runner priority";
      }
      break;
    }
    case fml::Thread::ThreadPriority::kDisplay: {
      OH_QoS_SetThreadQoS(QoS_Level::QOS_USER_INTERACTIVE);
      if (::setpriority(PRIO_PROCESS, 0, -1) != 0) {
        FML_LOG(ERROR) << "Failed to set UI task runner priority";
      }
      break;
    }
    case fml::Thread::ThreadPriority::kRaster: {
      OH_QoS_SetThreadQoS(QoS_Level::QOS_USER_INTERACTIVE);
      // Ohos describes -8 as "most important display threads, for
      // compositing the screen and retrieving input events". Conservatively
      // set the raster thread to slightly lower priority than it.
      if (::setpriority(PRIO_PROCESS, 0, -5) != 0) {
        // Defensive fallback. Depending on the OEM, it may not be possible
        // to set priority to -5.
        if (::setpriority(PRIO_PROCESS, 0, -2) != 0) {
          FML_LOG(ERROR) << "Failed to set raster task runner priority";
        }
      }
      break;
    }
    default:
      OH_QoS_SetThreadQoS(QoS_Level::QOS_DEFAULT);
      if (::setpriority(PRIO_PROCESS, 0, 0) != 0) {
        FML_LOG(ERROR) << "Failed to set priority";
      }
  }
}

static PlatformData GetDefaultPlatformData() {
  PlatformData platform_data;
  platform_data.lifecycle_state = "AppLifecycleState.detached";
  return platform_data;
}

OHOSShellHolder::OHOSShellHolder(
    const flutter::Settings& settings,
    std::shared_ptr<PlatformViewOHOSNAPI> napi_facade)
    : settings_(settings), napi_facade_(napi_facade) {
  static size_t thread_host_count = 1;
  auto thread_label = std::to_string(thread_host_count++);

  auto mask =
      ThreadHost::Type::kUi | ThreadHost::Type::kRaster | ThreadHost::Type::kIo;

  flutter::ThreadHost::ThreadHostConfig host_config(
      thread_label, mask, OHOSPlatformThreadConfigSetter);
  host_config.ui_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kUi, thread_label),
      fml::Thread::ThreadPriority::kDisplay);
  host_config.raster_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kRaster, thread_label),
      fml::Thread::ThreadPriority::kRaster);
  host_config.io_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kIo, thread_label),
      fml::Thread::ThreadPriority::kNormal);

  thread_host_ = std::make_shared<ThreadHost>(host_config);

  fml::WeakPtr<PlatformViewOHOS> weak_platform_view;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&napi_facade, &weak_platform_view](Shell& shell) {
        std::unique_ptr<PlatformViewOHOS> platform_view_ohos;
        platform_view_ohos = std::make_unique<PlatformViewOHOS>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            napi_facade,             // napi interop
            shell.GetSettings()
                .enable_software_rendering  // use software rendering
        );
        weak_platform_view = platform_view_ohos->GetWeakPtr();
        return platform_view_ohos;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  // The current thread will be used as the platform thread. Ensure that the
  // message loop is initialized.
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> raster_runner;
  fml::RefPtr<fml::TaskRunner> ui_runner;
  fml::RefPtr<fml::TaskRunner> io_runner;
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();
  raster_runner = thread_host_->raster_thread->GetTaskRunner();
  ui_runner = thread_host_->ui_thread->GetTaskRunner();
  io_runner = thread_host_->io_thread->GetTaskRunner();

  flutter::TaskRunners task_runners(thread_label,     // label
                                    platform_runner,  // platform
                                    raster_runner,    // raster
                                    ui_runner,        // ui
                                    io_runner         // io
  );

  shell_ =
      Shell::Create(GetDefaultPlatformData(),  // window data
                    task_runners,              // task runners
                    settings_,                 // settings
                    on_create_platform_view,   // platform view create callback
                    on_create_rasterizer       // rasterizer create callback
      );

  if (shell_) {
    shell_->GetDartVM()->GetConcurrentMessageLoop()->PostTaskToAllWorkers([]() {
      if (::setpriority(PRIO_PROCESS, gettid(), 1) != 0) {
        FML_LOG(ERROR) << "Failed to set Workers task runner priority";
      }
    });

    shell_->RegisterImageDecoder(
        [runner = task_runners.GetIOTaskRunner()](sk_sp<SkData> buffer) {
          return OHOSImageGenerator::MakeFromData(std::move(buffer), runner);
        },
        -1);

    FML_DLOG(INFO) << "Registered HarmonyOS SDK image decoder";
  }

  platform_view_ = weak_platform_view;
  FML_DCHECK(platform_view_);
}

OHOSShellHolder::OHOSShellHolder(
    const Settings& settings,
    const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade,
    const std::shared_ptr<ThreadHost>& thread_host,
    std::unique_ptr<Shell> shell,
    std::unique_ptr<OHOSAssetProvider> apk_asset_provider,
    const fml::WeakPtr<PlatformViewOHOS>& platform_view)
    : settings_(settings),
      napi_facade_(napi_facade),
      platform_view_(platform_view),
      thread_host_(thread_host),
      shell_(std::move(shell)),
      asset_provider_(std::move(apk_asset_provider)) {
  FML_DCHECK(napi_facade);
  FML_DCHECK(shell_);
  FML_DCHECK(shell_->IsSetup());
  FML_DCHECK(platform_view_);
  FML_DCHECK(thread_host_);
}

OHOSShellHolder::~OHOSShellHolder() {
  shell_.reset();
  thread_host_.reset();
}

bool OHOSShellHolder::IsValid() const {
  return shell_ != nullptr;
}

const flutter::Settings& OHOSShellHolder::GetSettings() const {
  return settings_;
}

std::unique_ptr<OHOSShellHolder> OHOSShellHolder::Spawn(
    std::shared_ptr<PlatformViewOHOSNAPI> napi_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args) const {
  FML_DCHECK(shell_ && shell_->IsSetup())
      << "A new Shell can only be spawned "
         "if the current Shell is properly constructed";

  // Pull out the new PlatformViewOHOS from the new Shell to feed to it to
  // the new OHOSShellHolder.
  //
  // It's a weak pointer because it's owned by the Shell (which we're also)
  // making below. And the OHOSShellHolder then owns the Shell.
  fml::WeakPtr<PlatformViewOHOS> weak_platform_view;

  // Take out the old OHOSContext to reuse inside the PlatformViewOHOS
  // of the new Shell.
  PlatformViewOHOS* ohos_platform_view = platform_view_.get();
  // There's some indirection with platform_view_ being a weak pointer but
  // we just checked that the shell_ exists above and a valid shell is the
  // owner of the platform view so this weak pointer always exists.
  FML_DCHECK(ohos_platform_view);
  std::shared_ptr<flutter::OHOSContext> ohos_context =
      ohos_platform_view->GetOHOSContext();
  FML_DCHECK(ohos_context);

  // This is a synchronous call, so the captures don't have race checks.
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&napi_facade, ohos_context, &weak_platform_view](Shell& shell) {
        std::unique_ptr<PlatformViewOHOS> platform_view_ohos;
        platform_view_ohos = std::make_unique<PlatformViewOHOS>(
            shell,                   // delegate
            shell.GetTaskRunners(),  // task runners
            napi_facade,             // JNI interop
            ohos_context             // OHOS context
        );
        weak_platform_view = platform_view_ohos->GetWeakPtr();
        return platform_view_ohos;
      };

  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  // TODO(xster): could be worth tracing this to investigate whether
  // the IsolateConfiguration could be cached somewhere.
  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    // If the RunConfiguration was null, the kernel blob wasn't readable.
    // Fail the whole thing.
    return nullptr;
  }

  std::unique_ptr<flutter::Shell> shell =
      shell_->Spawn(std::move(config.value()), initial_route,
                    on_create_platform_view, on_create_rasterizer);

  return std::unique_ptr<OHOSShellHolder>(new OHOSShellHolder(
      GetSettings(), napi_facade, thread_host_, std::move(shell),
      asset_provider_->Clone(), weak_platform_view));
}

void OHOSShellHolder::Launch(
    std::unique_ptr<OHOSAssetProvider> apk_asset_provider,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args) {
  if (!IsValid()) {
    return;
  }

  asset_provider_ = std::move(apk_asset_provider);
  auto config = BuildRunConfiguration(entrypoint, libraryUrl, entrypoint_args);
  if (!config) {
    return;
  }
  UpdateDisplayMetrics();
  shell_->RunEngine(std::move(config.value()));
}

fml::WeakPtr<PlatformViewOHOS> OHOSShellHolder::GetPlatformView() {
  FML_DCHECK(platform_view_);
  return platform_view_;
}

void OHOSShellHolder::NotifyLowMemoryWarning() {
  FML_CHECK(shell_);
  shell_->NotifyLowMemoryWarning();
}

std::optional<RunConfiguration> OHOSShellHolder::BuildRunConfiguration(
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::vector<std::string>& entrypoint_args) const {
  std::unique_ptr<IsolateConfiguration> isolate_configuration;
  if (flutter::DartVM::IsRunningPrecompiledCode()) {
    isolate_configuration = IsolateConfiguration::CreateForAppSnapshot();
  } else {
    std::unique_ptr<fml::Mapping> kernel_blob =
        fml::FileMapping::CreateReadOnly(
            GetSettings().application_kernel_asset);
    if (!kernel_blob) {
      FML_DLOG(ERROR) << "Unable to load the kernel blob asset.";
      return std::nullopt;
    }
    isolate_configuration =
        IsolateConfiguration::CreateForKernel(std::move(kernel_blob));
  }

  RunConfiguration config(std::move(isolate_configuration));
  config.AddAssetResolver(asset_provider_->Clone());

  {
    if (!entrypoint.empty() && !libraryUrl.empty()) {
      config.SetEntrypointAndLibrary(entrypoint, libraryUrl);
    } else if (!entrypoint.empty()) {
      config.SetEntrypoint(entrypoint);
    }
    if (!entrypoint_args.empty()) {
      config.SetEntrypointArgs(entrypoint_args);
    }
  }
  return config;
}

void OHOSShellHolder::UpdateDisplayMetrics() {
  std::vector<std::unique_ptr<Display>> displays;
  displays.push_back(std::make_unique<OHOSDisplay>(napi_facade_));
  shell_->OnDisplayUpdates(std::move(displays));
}

}  // namespace flutter
