// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FML_PLATFORM_OHOS_NAPI_UTIL_H_
#define FLUTTER_FML_PLATFORM_OHOS_NAPI_UTIL_H_

#include <napi/native_api.h>
#include <uv.h>
#include <vector>

#define NAPI_DEFAULT_METHOD(name, func) \
  { (name), nullptr, (func), nullptr, nullptr, nullptr, napi_default, nullptr }

namespace fml {
namespace napi {

void InitEnv(napi_env env);

uv_loop_t* AcquireLoopForThread();

napi_value InvokeJsMethod(napi_env env,
                          napi_ref ref,
                          const char* methodName,
                          size_t argc,
                          const napi_value* argv);

void* GetArrayBuffer(napi_env env, napi_value buf);

napi_value NewArrayBuffer(napi_env env, void* address, size_t size);

napi_value NewString(napi_env env,
                     const char* chars,
                     size_t size = NAPI_AUTO_LENGTH);

napi_value StringToJsString(napi_env env, const std::string& str);

std::string JsStringToString(napi_env env, napi_value str);

std::vector<std::string> StringArrayToVector(napi_env env, napi_value array);

napi_value VectorToStringArray(napi_env env,
                               const std::vector<std::string>& array);

napi_value FindClass(napi_env env, const char* name);

napi_value NewInstance(napi_env env,
                       napi_value clazz,
                       size_t argc,
                       const napi_value* argv);

napi_value NewObject(napi_env env,
                     const char* name,
                     size_t argc,
                     const napi_value* argv);

napi_value GetNamedProperty(napi_env env, napi_value obj, const char* key);

bool HasNamedProperty(napi_env env, napi_value obj, const char* key);

void SetNamedProperty(napi_env env,
                      napi_value obj,
                      const char* key,
                      napi_value value);

}  // namespace napi
}  // namespace fml

#endif  // FLUTTER_FML_PLATFORM_OHOS_NAPI_UTIL_H_
