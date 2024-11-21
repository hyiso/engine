// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/platform_view_ohos_napi_impl.h"

#include <dlfcn.h>
#include <native_image/native_image.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <window_manager/oh_display_manager.h>

#include <unicode/uchar.h>
#include <uv.h>

#include "flutter/common/constants.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/platform/ohos/napi_util.h"
#include "flutter/lib/ui/plugins/callback_cache.h"
#include "flutter/shell/platform/ohos/flutter_main.h"
#include "flutter/shell/platform/ohos/ohos_shell_holder.h"
#include "flutter/shell/platform/ohos/surface/ohos_native_window.h"

#include <GLES2/gl2ext.h>

#define OHOS_SHELL_HOLDER (reinterpret_cast<OHOSShellHolder*>(shell_holder))

namespace flutter {

SurfaceTexture::SurfaceTexture(int64_t tex_name) {
  native_image_ = OH_NativeImage_Create(tex_name, GL_TEXTURE_EXTERNAL_OES);
  native_window_ = OH_NativeImage_AcquireNativeWindow(native_image_);
  OH_NativeWindow_NativeWindowHandleOpt(native_window_, SET_TIMEOUT, 0);
}

SurfaceTexture::~SurfaceTexture() {
  if (native_image_ != nullptr) {
    native_image_ = nullptr;
  }
}

static napi_value GetSurfaceId(napi_env env, napi_callback_info info) {
  napi_value this_obj;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &this_obj, nullptr);
  SurfaceTexture* surface_texture;
  napi_unwrap(env, this_obj, reinterpret_cast<void**>(&surface_texture));
  uint64_t surface_id = 0;
  int32_t ret =
      OH_NativeImage_GetSurfaceId(surface_texture->native_image_, &surface_id);
  if (ret != 0) {
    FML_LOG(ERROR) << "OH_NativeImage_GetSurfaceId err:" << ret;
  }
  napi_value result;
  napi_create_int64(env, surface_id, &result);
  return result;
}

static napi_value SetDefaultBufferSize(napi_env env, napi_callback_info info) {
  napi_value this_obj;
  size_t argc = 2;
  napi_value args[2];
  napi_get_cb_info(env, info, &argc, args, &this_obj, nullptr);
  SurfaceTexture* surface_texture;
  napi_unwrap(env, this_obj, reinterpret_cast<void**>(&surface_texture));
  int32_t width;
  int32_t height;
  napi_get_value_int32(env, args[0], &width);
  napi_get_value_int32(env, args[1], &height);
  int code = SET_BUFFER_GEOMETRY;
  int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(
      surface_texture->native_window_, code, width, height);
  if (ret != 0) {
    FML_DLOG(ERROR) << "OH_NativeWindow_NativeWindowHandleOpt err:" << ret;
  }
  return nullptr;
}

static napi_value Release(napi_env env, napi_callback_info info) {
  napi_value this_obj;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &this_obj, nullptr);
  SurfaceTexture* surface_texture;
  napi_remove_wrap(env, this_obj, reinterpret_cast<void**>(&surface_texture));
  return nullptr;
}

static napi_value Constructor(napi_env env, napi_callback_info info) {
  napi_value js_this = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &js_this, nullptr);
  int64_t tex_name;
  napi_get_value_int64(env, args[0], &tex_name);
  void* surface_texture = nullptr;
  surface_texture = new SurfaceTexture(tex_name);
  napi_wrap(
      env, js_this, surface_texture,
      [](napi_env, void* finalize_data, void*) {
        SurfaceTexture* texture = static_cast<SurfaceTexture*>(finalize_data);
        delete texture;
      },
      nullptr, nullptr);
  return js_this;
}

static bool ExportSurfaceTexture(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      NAPI_DEFAULT_METHOD("getSurfaceId", GetSurfaceId),
      NAPI_DEFAULT_METHOD("setDefaultBufferSize", SetDefaultBufferSize),
      NAPI_DEFAULT_METHOD("release", Release),
  };
  napi_value constructor;
  auto status = napi_define_class(
      env, "SurfaceTexture", NAPI_AUTO_LENGTH, Constructor, nullptr,
      sizeof(desc) / sizeof(desc[0]), desc, &constructor);
  FML_CHECK(status == napi_ok) << "napi_define_class failed:" << status;
  status = napi_set_named_property(env, exports, "SurfaceTexture", constructor);
  FML_CHECK(status == napi_ok) << "napi_set_named_property failed:" << status;
  return true;
}

napi_value CreateFlutterCallbackInformation(
    napi_env env,
    const std::string& callbackName,
    const std::string& callbackClassName,
    const std::string& callbackLibraryPath) {
  int argc = 3;
  napi_value arguments[argc];
  arguments[0] = fml::napi::NewString(env, callbackName.c_str());
  arguments[1] = fml::napi::NewString(env, callbackClassName.c_str());
  arguments[2] = fml::napi::NewString(env, callbackLibraryPath.c_str());
  return fml::napi::NewObject(env, "FlutterCallbackInformation", argc,
                              arguments);
}

static napi_value AttachNAPI(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  FML_CHECK(status == napi_ok);

  std::shared_ptr<PlatformViewOHOSNAPIImpl> napi_facade =
      std::make_shared<PlatformViewOHOSNAPIImpl>(env, argv[0]);

  auto shell_holder = std::make_unique<OHOSShellHolder>(
      FlutterMain::Get().GetSettings(), napi_facade);
  if (shell_holder->IsValid()) {
    napi_value id;
    napi_create_int64(env, reinterpret_cast<int64_t>(shell_holder.release()),
                      &id);
    return id;
  } else {
    napi_value id;
    napi_create_int64(env, 0, &id);
    return id;
  }
}

static napi_value DestroyNAPI(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  FML_CHECK(status == napi_ok);

  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);
  FML_CHECK(status == napi_ok);

  delete OHOS_SHELL_HOLDER;
  return nullptr;
}

static napi_value SpawnNAPI(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 5;
  napi_value args[5] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  FML_CHECK(status == napi_ok);

  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);
  FML_CHECK(status == napi_ok);

  auto entrypoint = fml::napi::JsStringToString(env, args[1]);
  auto libraryUrl = fml::napi::JsStringToString(env, args[2]);
  auto initial_route = fml::napi::JsStringToString(env, args[3]);
  auto entrypoint_args = fml::napi::StringArrayToVector(env, args[4]);

  napi_value obj = fml::napi::NewObject(env, "FlutterNAPI", 0, nullptr);

  std::shared_ptr<PlatformViewOHOSNAPIImpl> napi_facade =
      std::make_shared<PlatformViewOHOSNAPIImpl>(env, obj);

  auto spawned_shell_holder = OHOS_SHELL_HOLDER->Spawn(
      napi_facade, entrypoint, libraryUrl, initial_route, entrypoint_args);

  if (spawned_shell_holder == nullptr || !spawned_shell_holder->IsValid()) {
    FML_LOG(ERROR) << "Could not spawn Shell";
    return nullptr;
  }

  napi_value shell_holder_id;
  napi_create_int64(env,
                    reinterpret_cast<int64_t>(spawned_shell_holder.release()),
                    &shell_holder_id);

  fml::napi::SetNamedProperty(env, obj, "nativeShellHolderId", shell_holder_id);
  return shell_holder_id;
}

static napi_value RunBundleAndSnapshotFromLibrary(napi_env env,
                                                  napi_callback_info info) {
  napi_status ret;
  size_t argc = 6;
  napi_value args[6] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t shell_holder;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  auto bundlePath = fml::napi::JsStringToString(env, args[1]);
  auto entrypoint = fml::napi::JsStringToString(env, args[2]);
  auto libraryUrl = fml::napi::JsStringToString(env, args[3]);
  NativeResourceManager* assetManager =
      OH_ResourceManager_InitNativeResourceManager(env, args[4]);

  auto entrypoint_args = fml::napi::StringArrayToVector(env, args[5]);

  auto ohos_asset_provider =
      std::make_unique<flutter::OHOSAssetProvider>(assetManager, bundlePath);

  OHOS_SHELL_HOLDER->Launch(std::move(ohos_asset_provider), entrypoint,
                            libraryUrl, entrypoint_args);
  return nullptr;
}

static napi_value LookupCallbackInformation(napi_env env,
                                            napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_status ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  FML_CHECK(ret == napi_ok);

  int64_t handle;
  bool lossless;
  ret = napi_get_value_bigint_int64(env, args[0], &handle, &lossless);
  FML_CHECK(ret == napi_ok);

  auto cbInfo = flutter::DartCallbackCache::GetCallbackInformation(handle);
  if (cbInfo == nullptr) {
    return nullptr;
  }

  return CreateFlutterCallbackInformation(env, cbInfo->name, cbInfo->class_name,
                                          cbInfo->library_path);
}

static napi_value SetViewportMetrics(napi_env env, napi_callback_info info) {
  napi_status ret;
  size_t argc = 20;
  napi_value args[20] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t shell_holder;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  double devicePixelRatio;
  ret = napi_get_value_double(env, args[1], &devicePixelRatio);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t physicalWidth;
  ret = napi_get_value_int64(env, args[2], &physicalWidth);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t physicalHeight;
  ret = napi_get_value_int64(env, args[3], &physicalHeight);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t physicalPaddingTop;
  ret = napi_get_value_int64(env, args[4], &physicalPaddingTop);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t physicalPaddingRight;
  ret = napi_get_value_int64(env, args[5], &physicalPaddingRight);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t physicalPaddingBottom;
  ret = napi_get_value_int64(env, args[6], &physicalPaddingBottom);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t physicalPaddingLeft;
  ret = napi_get_value_int64(env, args[7], &physicalPaddingLeft);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t physicalViewInsetTop;
  ret = napi_get_value_int64(env, args[8], &physicalViewInsetTop);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t physicalViewInsetRight;
  ret = napi_get_value_int64(env, args[9], &physicalViewInsetRight);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t physicalViewInsetBottom;
  ret = napi_get_value_int64(env, args[10], &physicalViewInsetBottom);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t physicalViewInsetLeft;
  ret = napi_get_value_int64(env, args[11], &physicalViewInsetLeft);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t systemGestureInsetTop;
  ret = napi_get_value_int64(env, args[12], &systemGestureInsetTop);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t systemGestureInsetRight;
  ret = napi_get_value_int64(env, args[13], &systemGestureInsetRight);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t systemGestureInsetBottom;
  ret = napi_get_value_int64(env, args[14], &systemGestureInsetBottom);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t systemGestureInsetLeft;
  ret = napi_get_value_int64(env, args[15], &systemGestureInsetLeft);
  if (ret != napi_ok) {
    return nullptr;
  }

  double physicalTouchSlop;
  ret = napi_get_value_double(env, args[16], &physicalTouchSlop);
  if (ret != napi_ok) {
    return nullptr;
  }

  std::vector<double> displayFeaturesBounds;
  napi_value array = args[17];
  uint32_t length;
  napi_get_array_length(env, array, &length);
  displayFeaturesBounds.resize(length);
  for (uint32_t i = 0; i < length; ++i) {
    napi_value element;
    napi_get_element(env, array, i, &element);
    napi_get_value_double(env, element, &(displayFeaturesBounds[i]));
  }

  std::vector<int64_t> displayFeaturesType;
  array = args[18];
  napi_get_array_length(env, array, &length);
  displayFeaturesType.resize(length);
  for (uint32_t i = 0; i < length; ++i) {
    napi_value element;
    napi_get_element(env, array, i, &element);
    napi_get_value_int64(env, element, &(displayFeaturesType[i]));
  }

  std::vector<int64_t> displayFeaturesState;
  array = args[19];
  napi_get_array_length(env, array, &length);
  displayFeaturesState.resize(length);
  for (uint32_t i = 0; i < length; ++i) {
    napi_value element;
    napi_get_element(env, array, i, &element);
    napi_get_value_int64(env, element, &(displayFeaturesState[i]));
  }

  const flutter::ViewportMetrics metrics{
      static_cast<double>(devicePixelRatio),
      static_cast<double>(physicalWidth),
      static_cast<double>(physicalHeight),
      static_cast<double>(physicalPaddingTop),
      static_cast<double>(physicalPaddingRight),
      static_cast<double>(physicalPaddingBottom),
      static_cast<double>(physicalPaddingLeft),
      static_cast<double>(physicalViewInsetTop),
      static_cast<double>(physicalViewInsetRight),
      static_cast<double>(physicalViewInsetBottom),
      static_cast<double>(physicalViewInsetLeft),
      static_cast<double>(systemGestureInsetTop),
      static_cast<double>(systemGestureInsetRight),
      static_cast<double>(systemGestureInsetBottom),
      static_cast<double>(systemGestureInsetLeft),
      static_cast<double>(physicalTouchSlop),
      displayFeaturesBounds,
      std::vector<int>(displayFeaturesType.begin(), displayFeaturesType.end()),
      std::vector<int>(displayFeaturesState.begin(),
                       displayFeaturesState.end()),
      0,
  };

  OHOS_SHELL_HOLDER->GetPlatformView()->SetViewportMetrics(
      kFlutterImplicitViewId, metrics);

  return nullptr;
}

static napi_value DispatchPlatformMessage(napi_env env,
                                          napi_callback_info info) {
  napi_status status;
  size_t argc = 5;
  napi_value args[5] = {nullptr};

  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 5 || status != napi_ok) {
    napi_throw_type_error(env, nullptr, "Invalid number of arguments");
    return nullptr;
  }
  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);
  if (status != napi_ok) {
    return nullptr;
  }
  std::string channel = fml::napi::JsStringToString(env, args[1]);
  void* message = fml::napi::GetArrayBuffer(env, args[2]);
  if (message == nullptr) {
    return nullptr;
  }
  int64_t position;
  status = napi_get_value_int64(env, args[3], &position);
  if (status != napi_ok) {
    return nullptr;
  }
  int64_t responseId;
  status = napi_get_value_int64(env, args[4], &responseId);
  if (status != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->GetPlatformView()->DispatchPlatformMessage(
      channel, message, position, responseId);
  return nullptr;
}

static napi_value DispatchEmptyPlatformMessage(napi_env env,
                                               napi_callback_info info) {
  napi_status status;
  size_t argc = 3;
  napi_value args[3] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);
  if (status != napi_ok) {
    return nullptr;
  }
  std::string channel = fml::napi::JsStringToString(env, args[1]);
  int64_t responseId;
  status = napi_get_value_int64(env, args[2], &responseId);
  if (status != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->GetPlatformView()->DispatchEmptyPlatformMessage(
      channel, responseId);
  return nullptr;
}

static napi_value CleanupMessageData(napi_env env, napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t messageData;
  ret = napi_get_value_int64(env, args[0], &messageData);
  if (ret != napi_ok) {
    return nullptr;
  }
  free(reinterpret_cast<void*>(messageData));
  return nullptr;
}

static napi_value DispatchPointerDataPacket(napi_env env,
                                            napi_callback_info info) {
  napi_status status;
  size_t argc = 3;
  napi_value args[3] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);
  FML_CHECK(status == napi_ok) << "Failed to get shell holder";

  void* buffer = fml::napi::GetArrayBuffer(env, args[1]);
  FML_CHECK(buffer != nullptr) << "Failed to get buffer";

  int64_t position;
  status = napi_get_value_int64(env, args[2], &position);
  FML_CHECK(status == napi_ok) << "Failed to get position";

  uint8_t* data = static_cast<uint8_t*>(buffer);
  auto packet = std::make_unique<flutter::PointerDataPacket>(data, position);
  OHOS_SHELL_HOLDER->GetPlatformView()->DispatchPointerDataPacket(
      std::move(packet));
  return nullptr;
}

static napi_value DispatchSemanticsAction(napi_env env,
                                          napi_callback_info info) {
  napi_status ret;
  size_t argc = 5;
  napi_value args[5] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder, id, action, position;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  ret = napi_get_value_int64(env, args[1], &id);
  if (ret != napi_ok) {
    return nullptr;
  }
  ret = napi_get_value_int64(env, args[2], &action);
  if (ret != napi_ok) {
    return nullptr;
  }
  void* actionData = fml::napi::GetArrayBuffer(env, args[3]);
  if (actionData == nullptr) {
    return nullptr;
  }
  ret = napi_get_value_int64(env, args[4], &position);
  if (ret != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->GetPlatformView()->DispatchSemanticsAction(
      id, action, actionData, position);
  return nullptr;
}

static napi_value SetSemanticsEnabled(napi_env env, napi_callback_info info) {
  napi_status ret;
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder;
  bool enabled;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  ret = napi_get_value_bool(env, args[1], &enabled);
  if (ret != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->GetPlatformView()->SetSemanticsEnabled(enabled);
  return nullptr;
}

napi_value SetAccessibilityFeatures(napi_env env, napi_callback_info info) {
  napi_status ret;
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t shell_holder;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t flags;
  ret = napi_get_value_int64(env, args[1], &flags);
  if (ret != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->GetPlatformView()->SetAccessibilityFeatures(flags);
  return nullptr;
}

static napi_value GetIsSoftwareRendering(napi_env env,
                                         napi_callback_info info) {
  napi_value result = nullptr;
  napi_status ret = napi_get_boolean(
      env, FlutterMain::Get().GetSettings().enable_software_rendering, &result);
  if (ret != napi_ok) {
    return nullptr;
  }
  return result;
}

static napi_value RegisterTexture(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder;
  napi_get_value_int64(env, args[0], &shell_holder);
  int64_t texture_id;
  napi_get_value_int64(env, args[1], &texture_id);
  uint64_t surface_id = 0;
  OH_NativeImage* native_image =
      OH_NativeImage_Create(texture_id, GL_TEXTURE_EXTERNAL_OES);
  if (native_image == nullptr) {
    FML_LOG(ERROR) << "PlatformViewOHOS::RegisterExternalTexture Error with "
                      "OH_NativeImage_Create";
  }
  OHNativeWindow* native_window =
      OH_NativeImage_AcquireNativeWindow(native_image);
  if (native_window == nullptr) {
    FML_DLOG(ERROR) << "OHOSExternalTextureGL::setTextureBufferSize "
                       "native_window is nullptr";
  }
  int32_t ret =
      OH_NativeWindow_NativeWindowHandleOpt(native_window, SET_TIMEOUT, 0);
  if (ret != 0) {
    FML_DLOG(ERROR) << "OH_NativeWindow_NativeWindowHandleOpt err:" << ret;
  }
  OHOS_SHELL_HOLDER->GetPlatformView()->RegisterExternalTexture(texture_id,
                                                                native_image);
  ret = OH_NativeImage_GetSurfaceId(native_image, &surface_id);
  if (ret != 0) {
    FML_LOG(ERROR) << "OH_NativeImage_GetSurfaceId err:" << ret;
  }
  napi_value id;
  napi_create_int64(env, surface_id, &id);
  return id;
}

static napi_value RegisterSurfaceTexture(napi_env env,
                                         napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder;
  napi_get_value_int64(env, args[0], &shell_holder);
  int64_t texture_id;
  napi_get_value_int64(env, args[1], &texture_id);
  napi_value js_surface_texture = args[2];
  SurfaceTexture* surface_texture;
  napi_unwrap(env, js_surface_texture,
              reinterpret_cast<void**>(&surface_texture));
  OHOS_SHELL_HOLDER->GetPlatformView()->RegisterExternalTexture(
      texture_id, surface_texture->native_image_);
  return nullptr;
}

static napi_value UnregisterTexture(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  int64_t shell_holder;
  int64_t textureId;
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  napi_get_value_int64(env, args[0], &shell_holder);
  napi_get_value_int64(env, args[1], &textureId);
  OHOS_SHELL_HOLDER->GetPlatformView()->UnRegisterExternalTexture(textureId);
  return nullptr;
}

static napi_value InvokePlatformMessageResponseCallback(
    napi_env env,
    napi_callback_info info) {
  napi_status ret;
  size_t argc = 4;
  napi_value args[4] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder, responseId, position;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  ret = napi_get_value_int64(env, args[1], &responseId);
  if (ret != napi_ok) {
    return nullptr;
  }
  void* message = fml::napi::GetArrayBuffer(env, args[2]);
  if (message == nullptr) {
    return nullptr;
  }
  ret = napi_get_value_int64(env, args[3], &position);
  if (ret != napi_ok) {
    return nullptr;
  }

  uint8_t* response_data = static_cast<uint8_t*>(message);
  FML_DCHECK(response_data != nullptr);
  auto mapping = std::make_unique<fml::MallocMapping>(
      fml::MallocMapping::Copy(response_data, response_data + position));
  OHOS_SHELL_HOLDER->GetPlatformMessageHandler()
      ->InvokePlatformMessageResponseCallback(responseId, std::move(mapping));
  return nullptr;
}

static napi_value InvokePlatformMessageEmptyResponseCallback(
    napi_env env,
    napi_callback_info info) {
  napi_status ret;
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int64_t shell_holder, responseId;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  ret = napi_get_value_int64(env, args[1], &responseId);
  if (ret != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(responseId);
  return nullptr;
}

static napi_value NotifyLowMemoryWarning(napi_env env,
                                         napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t shell_holder;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  OHOS_SHELL_HOLDER->NotifyLowMemoryWarning();
  return nullptr;
}

static napi_value FlutterTextUtilsIsEmoji(napi_env env,
                                          napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t codePoint;
  ret = napi_get_value_int64(env, args[0], &codePoint);
  if (ret != napi_ok) {
    return nullptr;
  }
  bool value = u_hasBinaryProperty(codePoint, UProperty::UCHAR_EMOJI);
  napi_value result = nullptr;
  ret = napi_get_boolean(env, value, &result);
  if (ret != napi_ok) {
    return nullptr;
  }

  return result;
}

static napi_value FlutterTextUtilsIsEmojiModifier(napi_env env,
                                                  napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t codePoint;
  ret = napi_get_value_int64(env, args[0], &codePoint);
  if (ret != napi_ok) {
    return nullptr;
  }
  bool value = u_hasBinaryProperty(codePoint, UProperty::UCHAR_EMOJI_MODIFIER);
  napi_value result = nullptr;
  ret = napi_get_boolean(env, value, &result);
  if (ret != napi_ok) {
    return nullptr;
  }

  return result;
}

static napi_value FlutterTextUtilsIsEmojiModifierBase(napi_env env,
                                                      napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t codePoint;
  ret = napi_get_value_int64(env, args[0], &codePoint);
  if (ret != napi_ok) {
    return nullptr;
  }
  bool value =
      u_hasBinaryProperty(codePoint, UProperty::UCHAR_EMOJI_MODIFIER_BASE);
  napi_value result = nullptr;
  ret = napi_get_boolean(env, value, &result);
  if (ret != napi_ok) {
    return nullptr;
  }

  return result;
}

static napi_value FlutterTextUtilsIsVariationSelector(napi_env env,
                                                      napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t codePoint;
  ret = napi_get_value_int64(env, args[0], &codePoint);
  if (ret != napi_ok) {
    return nullptr;
  }
  bool value =
      u_hasBinaryProperty(codePoint, UProperty::UCHAR_VARIATION_SELECTOR);
  napi_value result = nullptr;
  ret = napi_get_boolean(env, value, &result);
  if (ret != napi_ok) {
    return nullptr;
  }

  return result;
}

static napi_value FlutterTextUtilsIsRegionalIndicator(napi_env env,
                                                      napi_callback_info info) {
  napi_status ret;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t codePoint;
  ret = napi_get_value_int64(env, args[0], &codePoint);
  if (ret != napi_ok) {
    return nullptr;
  }
  bool value =
      u_hasBinaryProperty(codePoint, UProperty::UCHAR_REGIONAL_INDICATOR);
  napi_value result = nullptr;
  ret = napi_get_boolean(env, value, &result);
  if (ret != napi_ok) {
    return nullptr;
  }

  return result;
}

static void LoadLoadingUnitFailure(intptr_t loading_unit_id,
                                   const std::string& message,
                                   bool transient) {
  // TODO(garyq): Implement
}

static napi_value LoadDartDeferredLibrary(napi_env env,
                                          napi_callback_info info) {
  napi_status ret;
  size_t argc = 3;
  napi_value args[3] = {nullptr};
  ret = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (ret != napi_ok) {
    return nullptr;
  }

  int64_t shell_holder;
  ret = napi_get_value_int64(env, args[0], &shell_holder);
  if (ret != napi_ok) {
    return nullptr;
  }
  int64_t loadingUnitId;
  ret = napi_get_value_int64(env, args[1], &loadingUnitId);
  if (ret != napi_ok) {
    return nullptr;
  }
  intptr_t loading_unit_id = static_cast<intptr_t>(loadingUnitId);
  std::vector<std::string> search_paths =
      fml::napi::StringArrayToVector(env, args[2]);

  // Use dlopen here to directly check if handle is nullptr before creating a
  // NativeLibrary.
  void* handle = nullptr;
  while (handle == nullptr && !search_paths.empty()) {
    std::string path = search_paths.back();
    handle = ::dlopen(path.c_str(), RTLD_NOW);
    search_paths.pop_back();
  }
  if (handle == nullptr) {
    LoadLoadingUnitFailure(loading_unit_id,
                           "No lib .so found for provided search paths.", true);
    return nullptr;
  }
  fml::RefPtr<fml::NativeLibrary> native_lib =
      fml::NativeLibrary::CreateWithHandle(handle, false);

  // Resolve symbols.
  std::unique_ptr<const fml::SymbolMapping> data_mapping =
      std::make_unique<const fml::SymbolMapping>(
          native_lib, DartSnapshot::kIsolateDataSymbol);
  std::unique_ptr<const fml::SymbolMapping> instructions_mapping =
      std::make_unique<const fml::SymbolMapping>(
          native_lib, DartSnapshot::kIsolateInstructionsSymbol);

  OHOS_SHELL_HOLDER->GetPlatformView()->LoadDartDeferredLibrary(
      loading_unit_id, std::move(data_mapping),
      std::move(instructions_mapping));

  return nullptr;
}

static napi_value SetTextureBufferSize(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4] = {nullptr};
  int64_t shell_holder;
  int64_t textureId;
  int32_t width;
  int32_t height;
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  napi_get_value_int64(env, args[0], &shell_holder);
  napi_get_value_int64(env, args[1], &textureId);
  napi_get_value_int32(env, args[2], &width);
  napi_get_value_int32(env, args[3], &height);
  OHOS_SHELL_HOLDER->GetPlatformView()->SetTextureBufferSize(textureId, width,
                                                             height);
  return nullptr;
}

static napi_value SurfaceCreated(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  FML_CHECK(status == napi_ok) << "Failed to get callback info";

  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);

  int64_t surface_id;
  bool lossLess = false;
  status = napi_get_value_bigint_int64(env, args[1], &surface_id, &lossLess);
  FML_CHECK(status == napi_ok) << "Failed to get surface id";

  OHNativeWindow* window = nullptr;
  int32_t ret =
      OH_NativeWindow_CreateNativeWindowFromSurfaceId(surface_id, &window);
  FML_CHECK(ret == 0) << "Failed to create native window from surface id";

  auto native_window = fml::MakeRefCounted<OHOSNativeWindow>(window);
  OHOS_SHELL_HOLDER->GetPlatformView()->NotifyCreated(std::move(native_window));
  return nullptr;
}

static napi_value SurfaceChanged(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 3;
  napi_value args[3] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  FML_CHECK(status == napi_ok) << "Failed to get callback info";

  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);

  int32_t width;
  status = napi_get_value_int32(env, args[1], &width);
  int32_t height;
  status = napi_get_value_int32(env, args[2], &height);

  OHOS_SHELL_HOLDER->GetPlatformView()->NotifyChanged(
      SkISize::Make(width, height));
  return nullptr;
}

static napi_value SurfaceDestroyed(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  FML_CHECK(status == napi_ok) << "Failed to get callback info";

  int64_t shell_holder;
  status = napi_get_value_int64(env, args[0], &shell_holder);

  OHOS_SHELL_HOLDER->GetPlatformView()->NotifyDestroyed();
  return nullptr;
}

static napi_value EncodeUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], nullptr, 0, &length);

  auto null_terminated_length = length + 1;
  auto char_array = std::make_unique<char[]>(null_terminated_length);
  napi_get_value_string_utf8(env, args[0], char_array.get(),
                             null_terminated_length, nullptr);

  void* data;
  napi_value arraybuffer;
  napi_create_arraybuffer(env, length, &data, &arraybuffer);
  std::memcpy(data, char_array.get(), length);

  napi_value uint8_array;
  napi_create_typedarray(env, napi_uint8_array, length, arraybuffer, 0,
                         &uint8_array);
  return uint8_array;
}

static napi_value DecodeUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  size_t size = 0;
  void* data = nullptr;
  napi_get_typedarray_info(env, args[0], nullptr, &size, &data, nullptr,
                           nullptr);
  return fml::napi::NewString(env, static_cast<char*>(data), size);
}

bool RegisterApi(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      // Start of methods from FlutterNAPI
      NAPI_DEFAULT_METHOD("nativeAttach", AttachNAPI),
      NAPI_DEFAULT_METHOD("nativeDestroy", DestroyNAPI),
      NAPI_DEFAULT_METHOD("nativeSpawn", SpawnNAPI),
      NAPI_DEFAULT_METHOD("nativeRunBundleAndSnapshotFromLibrary",
                          RunBundleAndSnapshotFromLibrary),
      NAPI_DEFAULT_METHOD("nativeDispatchEmptyPlatformMessage",
                          DispatchEmptyPlatformMessage),
      NAPI_DEFAULT_METHOD("nativeCleanupMessageData", CleanupMessageData),
      NAPI_DEFAULT_METHOD("nativeDispatchPlatformMessage",
                          DispatchPlatformMessage),
      NAPI_DEFAULT_METHOD("nativeInvokePlatformMessageResponseCallback",
                          InvokePlatformMessageResponseCallback),
      NAPI_DEFAULT_METHOD("nativeInvokePlatformMessageEmptyResponseCallback",
                          InvokePlatformMessageEmptyResponseCallback),
      NAPI_DEFAULT_METHOD("nativeNotifyLowMemoryWarning",
                          NotifyLowMemoryWarning),

      // Start of methods from FlutterView
      NAPI_DEFAULT_METHOD("nativeSurfaceCreated", SurfaceCreated),
      NAPI_DEFAULT_METHOD("nativeSurfaceChanged", SurfaceChanged),
      NAPI_DEFAULT_METHOD("nativeSurfaceDestroyed", SurfaceDestroyed),
      NAPI_DEFAULT_METHOD("nativeSetViewportMetrics", SetViewportMetrics),
      NAPI_DEFAULT_METHOD("nativeDispatchPointerDataPacket",
                          DispatchPointerDataPacket),
      NAPI_DEFAULT_METHOD("nativeDispatchSemanticsAction",
                          DispatchSemanticsAction),
      NAPI_DEFAULT_METHOD("nativeSetSemanticsEnabled", SetSemanticsEnabled),
      NAPI_DEFAULT_METHOD("nativeSetAccessibilityFeatures",
                          SetAccessibilityFeatures),
      NAPI_DEFAULT_METHOD("nativeGetIsSoftwareRenderingEnabled",
                          GetIsSoftwareRendering),

      NAPI_DEFAULT_METHOD("nativeLoadDartDeferredLibrary",
                          LoadDartDeferredLibrary),
      NAPI_DEFAULT_METHOD("nativeFlutterTextUtilsIsEmoji",
                          FlutterTextUtilsIsEmoji),
      NAPI_DEFAULT_METHOD("nativeFlutterTextUtilsIsEmojiModifier",
                          FlutterTextUtilsIsEmojiModifier),
      NAPI_DEFAULT_METHOD("nativeFlutterTextUtilsIsEmojiModifierBase",
                          FlutterTextUtilsIsEmojiModifierBase),
      NAPI_DEFAULT_METHOD("nativeFlutterTextUtilsIsVariationSelector",
                          FlutterTextUtilsIsVariationSelector),
      NAPI_DEFAULT_METHOD("nativeFlutterTextUtilsIsRegionalIndicator",
                          FlutterTextUtilsIsRegionalIndicator),
      NAPI_DEFAULT_METHOD("nativeRegisterTexture", RegisterTexture),
      NAPI_DEFAULT_METHOD("nativeRegisterSurfaceTexture",
                          RegisterSurfaceTexture),
      NAPI_DEFAULT_METHOD("nativeSetTextureBufferSize", SetTextureBufferSize),
      NAPI_DEFAULT_METHOD("nativeUnregisterTexture", UnregisterTexture),
      NAPI_DEFAULT_METHOD("nativeEncodeUtf8", EncodeUtf8),
      NAPI_DEFAULT_METHOD("nativeDecodeUtf8", DecodeUtf8),
      NAPI_DEFAULT_METHOD("nativeLookupCallbackInformation",
                          LookupCallbackInformation),
  };

  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  return true;
}

bool PlatformViewOHOS::Register(napi_env env, napi_value exports) {
  return RegisterApi(env, exports) && ExportSurfaceTexture(env, exports);
}

PlatformViewOHOSNAPIImpl::PlatformViewOHOSNAPIImpl(napi_env env, napi_value obj)
    : env_(env) {
  napi_create_reference(env, obj, 1, &ref_);
}

PlatformViewOHOSNAPIImpl::~PlatformViewOHOSNAPIImpl() = default;

void PlatformViewOHOSNAPIImpl::FlutterViewHandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message,
    int responseId) {
  napi_status status;
  napi_value params[4];
  params[0] = fml::napi::StringToJsString(env_, message->channel());
  status = napi_create_int64(env_, responseId, &params[2]);
  if (status != napi_ok) {
    return;
  }
  if (message->hasData()) {
    params[1] = fml::napi::NewArrayBuffer(
        env_, (void*)message->data().GetMapping(), message->data().GetSize());
    // Message data is deleted in CleanupMessageData.
    fml::MallocMapping mapping = message->releaseData();
    int64_t messageData = reinterpret_cast<int64_t>(mapping.Release());
    status = napi_create_int64(env_, messageData, &params[3]);
    FML_DCHECK(status == napi_ok) << "Failed to napi_create_int64:" << status;
  } else {
    params[1] = nullptr;
    params[3] = nullptr;
  }

  fml::napi::InvokeJsMethod(env_, ref_, "handlePlatformMessage", 4, params);
}

void PlatformViewOHOSNAPIImpl::FlutterViewHandlePlatformMessageResponse(
    int responseId,
    std::unique_ptr<fml::Mapping> data) {
  napi_value params[2];
  napi_status status = napi_create_int64(env_, responseId, params);
  if (status != napi_ok) {
    return;
  }

  if (data == nullptr) {
    params[1] = nullptr;
  } else {
    params[1] = fml::napi::NewArrayBuffer(env_, (void*)data->GetMapping(),
                                          data->GetSize());
  }

  fml::napi::InvokeJsMethod(env_, ref_, "handlePlatformMessageResponse", 2,
                            params);
}

double PlatformViewOHOSNAPIImpl::FlutterViewGetScaledFontSize(
    double font_size,
    int configuration_id) const {
  FML_DLOG(INFO) << "PlatformViewOHOSNAPIImpl::FlutterViewGetScaledFontSize"
                 << " font_size:" << font_size
                 << ", configuration_id:" << configuration_id;
  napi_value params[2];
  napi_status status = napi_create_double(env_, font_size, &params[0]);
  if (status != napi_ok) {
    return -1;
  }
  status = napi_create_int32(env_, configuration_id, &params[1]);
  if (status != napi_ok) {
    return -1;
  }
  napi_value result =
      fml::napi::InvokeJsMethod(env_, ref_, "getScaledFontSize", 2, params);
  double scaled_font_size;
  napi_get_value_double(env_, result, &scaled_font_size);
  FML_DLOG(INFO) << "PlatformViewOHOSNAPIImpl::FlutterViewGetScaledFontSize"
                 << " scaled_font_size:" << scaled_font_size;
  return scaled_font_size;
}

void PlatformViewOHOSNAPIImpl::FlutterViewOnFirstFrame() {
  fml::napi::InvokeJsMethod(env_, ref_, "onFirstFrame", 0, nullptr);
}

void PlatformViewOHOSNAPIImpl::FlutterViewOnPreEngineRestart() {
  fml::napi::InvokeJsMethod(env_, ref_, "onPreEngineRestart", 0, nullptr);
}

std::unique_ptr<std::vector<std::string>>
PlatformViewOHOSNAPIImpl::FlutterViewComputePlatformResolvedLocale(
    std::vector<std::string> supported_locales_data) {
  std::unique_ptr<std::vector<std::string>> out =
      std::make_unique<std::vector<std::string>>();
  napi_value locales_data =
      fml::napi::VectorToStringArray(env_, supported_locales_data);
  napi_value result = fml::napi::InvokeJsMethod(
      env_, ref_, "computePlatformResolvedLocale", 1, &locales_data);
  uint32_t length;
  napi_status status = napi_get_array_length(env_, result, &length);
  FML_DCHECK(status == napi_ok) << "Failed to napi_get_array_length:" << status;
  for (uint32_t i = 0; i < length; i++) {
    napi_value element;
    napi_get_element(env_, result, i, &element);
    out->emplace_back(fml::napi::JsStringToString(env_, element));
  }
  FML_LOG(ERROR) << "FlutterViewComputePlatformResolvedLocale out size:"
                 << out->size();
  for (const auto& item : *out) {
    FML_LOG(ERROR) << "FlutterViewComputePlatformResolvedLocale out item:"
                   << item;
  }
  return out;
}

double PlatformViewOHOSNAPIImpl::GetDisplayRefreshRate() {
  uint32_t refreshRate = 60;
  OH_NativeDisplayManager_GetDefaultDisplayRefreshRate(&refreshRate);
  return static_cast<double>(refreshRate);
}

double PlatformViewOHOSNAPIImpl::GetDisplayWidth() {
  int32_t width = 0;
  OH_NativeDisplayManager_GetDefaultDisplayWidth(&width);
  return static_cast<double>(width);
}

double PlatformViewOHOSNAPIImpl::GetDisplayHeight() {
  int32_t height = 0;
  OH_NativeDisplayManager_GetDefaultDisplayHeight(&height);
  return static_cast<double>(height);
}

double PlatformViewOHOSNAPIImpl::GetDisplayDensity() {
  float density = 2.0;
  OH_NativeDisplayManager_GetDefaultDisplayDensityPixels(&density);
  return static_cast<double>(density);
}

bool PlatformViewOHOSNAPIImpl::RequestDartDeferredLibrary(int loading_unit_id) {
  return true;
}

}  // namespace flutter
