// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <hilog/log.h>
#include <optional>
#include <vector>

#include "common/settings.h"
#include "flutter/fml/command_line.h"
#include "flutter/fml/file.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/native_library.h"
#include "flutter/fml/paths.h"
#include "flutter/fml/platform/ohos/napi_util.h"
#include "flutter/fml/platform/ohos/paths_ohos.h"
#include "flutter/fml/size.h"
#include "flutter/lib/ui/plugins/callback_cache.h"
#include "flutter/runtime/dart_vm.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/switches.h"
#include "flutter/shell/platform/ohos/flutter_main.h"
#include "flutter/shell/platform/ohos/ohos_context_vulkan_impeller.h"
#include "impeller/base/validation.h"
#include "third_party/dart/runtime/include/dart_tools_api.h"
#include "txt/platform.h"

namespace flutter {

extern "C" {
#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
// Used for debugging dart:* sources.
extern const uint8_t kPlatformStrongDill[];
extern const intptr_t kPlatformStrongDillSize;
#endif
}

FlutterMain::FlutterMain(const flutter::Settings& settings)
    : settings_(settings) {}

FlutterMain::~FlutterMain() = default;

static std::unique_ptr<FlutterMain> g_flutter_main;

FlutterMain& FlutterMain::Get() {
  FML_CHECK(g_flutter_main) << "ensureInitializationComplete must have already "
                               "been called.";
  return *g_flutter_main;
}

const flutter::Settings& FlutterMain::GetSettings() const {
  return settings_;
}

napi_value FlutterMain::Init(napi_env env, napi_callback_info info) {
  size_t argc = 6;
  napi_value argv[argc];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  napi_value shellArgs = argv[1];
  napi_value kernelPath = argv[2];
  napi_value appStoragePath = argv[3];
  napi_value engineCachesPath = argv[4];

  std::vector<std::string> args;
  args.push_back("flutter");
  for (auto& arg : fml::napi::StringArrayToVector(env, shellArgs)) {
    args.push_back(arg);
  }
  auto command_line = fml::CommandLineFromIterators(args.begin(), args.end());

  auto settings = SettingsFromCommandLine(command_line);

  settings.ohos_rendering_api = SelectedRenderingAPI(settings);
  switch (settings.ohos_rendering_api) {
    case OHOSRenderingAPI::kSoftware:
    case OHOSRenderingAPI::kSkiaOpenGLES:
      settings.enable_impeller = false;
      break;
    case OHOSRenderingAPI::kImpellerOpenGLES:
    case OHOSRenderingAPI::kImpellerVulkan:
      settings.enable_impeller = true;
      break;
  }

  // Restore the callback cache.
  // TODO(chinmaygarde): Route all cache file access through FML and remove this
  // setter.
  flutter::DartCallbackCache::SetCachePath(
      fml::napi::JsStringToString(env, appStoragePath));

  fml::paths::InitializeFlutterCachesPath(
      fml::napi::JsStringToString(env, engineCachesPath));

  flutter::DartCallbackCache::LoadCacheFromDisk();

  if (!flutter::DartVM::IsRunningPrecompiledCode() && kernelPath) {
    // Check to see if the appropriate kernel files are present and configure
    // settings accordingly.
    auto application_kernel_path = fml::napi::JsStringToString(env, kernelPath);

    if (fml::IsFile(application_kernel_path)) {
      settings.application_kernel_asset = application_kernel_path;
    }
  }

  settings.task_observer_add = [](intptr_t key, const fml::closure& callback) {
    fml::MessageLoop::GetCurrent().AddTaskObserver(key, callback);
  };

  settings.task_observer_remove = [](intptr_t key) {
    fml::MessageLoop::GetCurrent().RemoveTaskObserver(key);
  };

  settings.log_message_callback = [](const std::string& tag,
                                     const std::string& message) {
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, tag.c_str(), "%{public}s",
                 message.c_str());
  };

  settings.enable_platform_isolates = true;

#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
  // There are no ownership concerns here as all mappings are owned by the
  // embedder and not the engine.
  auto make_mapping_callback = [](const uint8_t* mapping, size_t size) {
    return [mapping, size]() {
      return std::make_unique<fml::NonOwnedMapping>(mapping, size);
    };
  };

  settings.dart_library_sources_kernel =
      make_mapping_callback(kPlatformStrongDill, kPlatformStrongDillSize);
#endif  // FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG

  g_flutter_main.reset(new FlutterMain(settings));

  g_flutter_main->SetupDartVMServiceUriCallback(env);

  return nullptr;
}

void FlutterMain::SetupDartVMServiceUriCallback(napi_env env) {
  auto set_uri = [env](const std::string& uri) {
    napi_value flutter_napi_class = fml::napi::FindClass(env, "FlutterNAPI");
    if (!flutter_napi_class) {
      return;
    }
    napi_value js_uri = fml::napi::StringToJsString(env, uri);
    fml::napi::SetNamedProperty(env, flutter_napi_class, "vmServiceUri",
                                js_uri);
  };

  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();

  vm_service_uri_callback_ = DartServiceIsolate::AddServerStatusCallback(
      [platform_runner, set_uri](const std::string& uri) {
        platform_runner->PostTask([uri, set_uri] { set_uri(uri); });
      });
}

static napi_value PrefetchDefaultFontManager(napi_env env,
                                             napi_callback_info info) {
  // Initialize a singleton owned by Skia.
  txt::GetDefaultFontManager();
  return nullptr;
}

bool FlutterMain::Register(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      NAPI_DEFAULT_METHOD("nativeInit", Init),
      NAPI_DEFAULT_METHOD("nativePrefetchDefaultFontManager",
                          PrefetchDefaultFontManager),
  };

  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  return true;
}

// static
OHOSRenderingAPI FlutterMain::SelectedRenderingAPI(
    const flutter::Settings& settings) {
  if (settings.enable_software_rendering) {
    FML_CHECK(!settings.enable_impeller)
        << "Impeller does not support software rendering. Either disable "
           "software rendering or disable impeller.";
    return OHOSRenderingAPI::kSoftware;
  }
  constexpr OHOSRenderingAPI kVulkanUnsupportedFallback =
      OHOSRenderingAPI::kSkiaOpenGLES;

  // Debug/Profile only functionality for testing a specific
  // backend configuration.
#ifndef FLUTTER_RELEASE
  if (settings.requested_rendering_backend == "opengles" &&
      settings.enable_impeller) {
    return OHOSRenderingAPI::kImpellerOpenGLES;
  }
  if (settings.requested_rendering_backend == "vulkan" &&
      settings.enable_impeller) {
    return OHOSRenderingAPI::kImpellerVulkan;
  }
#endif

  if (settings.enable_impeller) {
    // Determine if Vulkan is supported by creating a Vulkan context and
    // checking if it is valid.
    impeller::ScopedValidationDisable disable_validation;
    auto vulkan_backend = std::make_unique<OHOSContextVulkanImpeller>(
        /*enable_vulkan_validation=*/false,
        /*enable_vulkan_gpu_tracing=*/false,
        /*quiet=*/true);
    if (!vulkan_backend->IsValid()) {
      return kVulkanUnsupportedFallback;
    }
    return OHOSRenderingAPI::kImpellerVulkan;
  }

  return OHOSRenderingAPI::kSkiaOpenGLES;
}

}  // namespace flutter
