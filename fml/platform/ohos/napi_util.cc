// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/platform/ohos/napi_util.h"

#include <memory>
#include <string>

#include "flutter/fml/logging.h"

namespace fml {
namespace napi {

static thread_local napi_env tls_napi_env = nullptr;

void InitEnv(napi_env env) {
  tls_napi_env = env;
}

uv_loop_t* AcquireLoopForThread() {
  uv_loop_t* loop;
  if (tls_napi_env == nullptr) {
    loop = new uv_loop_t;
    uv_loop_init(loop);
  } else {
    napi_status status;
    status = napi_get_uv_event_loop(tls_napi_env, &loop);
    FML_CHECK(status == napi_ok);
  }
  return loop;
}

void* GetArrayBuffer(napi_env env, napi_value buf) {
  void* address;
  size_t length = 0;
  napi_status status = napi_get_arraybuffer_info(env, buf, &address, &length);
  FML_DCHECK(status == napi_ok) << "napi_get_arraybuffer_info failed";
  return address;
}

napi_value NewArrayBuffer(napi_env env, void* address, size_t size) {
  void* data = nullptr;
  napi_value buffer = nullptr;
  napi_status status = napi_create_arraybuffer(env, size, &data, &buffer);
  FML_DCHECK(status == napi_ok) << "napi_create_arraybuffer failed";
  memcpy(data, address, size);
  return buffer;
}

napi_value VectorToStringArray(napi_env env,
                               const std::vector<std::string>& array) {
  napi_value result;
  napi_create_array(env, &result);
  for (size_t i = 0; i < array.size(); i++) {
    napi_set_element(env, result, i, StringToJsString(env, array[i]));
  }
  return result;
}

napi_value InvokeJsMethod(napi_env env,
                          napi_ref ref,
                          const char* methodName,
                          size_t argc,
                          const napi_value* argv) {
  napi_value obj, fn, ret;
  napi_status status = napi_get_reference_value(env, ref, &obj);
  FML_DCHECK(status == napi_ok) << "napi_get_reference_value failed";
  status = napi_get_named_property(env, obj, methodName, &fn);
  FML_DCHECK(status == napi_ok)
      << "napi_get_named_property failed, methodName: " << methodName;
  status = napi_call_function(env, obj, fn, argc, argv, &ret);
  FML_DCHECK(status == napi_ok)
      << "napi_call_function failed, methodName: " << methodName;
  return ret;
}

napi_value NewString(napi_env env, const char* chars, size_t size) {
  napi_value result;
  napi_create_string_utf8(env, chars, size, &result);
  return result;
}

napi_value StringToJsString(napi_env env, const std::string& str) {
  return NewString(env, str.c_str(), str.size());
}

std::string JsStringToString(napi_env env, napi_value str) {
  if (str == nullptr) {
    return "";
  }
  size_t str_len;
  napi_status status;
  status = napi_get_value_string_utf8(env, str, nullptr, 0, &str_len);
  if (status != napi_ok) {
    return "";
  }
  std::vector<char> chars(str_len + 1);
  status =
      napi_get_value_string_utf8(env, str, chars.data(), str_len + 1, &str_len);
  if (status != napi_ok) {
    return "";
  }
  return std::string(chars.data(), str_len);
}

std::vector<std::string> StringArrayToVector(napi_env env, napi_value array) {
  uint32_t array_length;
  napi_get_array_length(env, array, &array_length);
  std::vector<std::string> out;
  for (uint32_t i = 0; i < array_length; i++) {
    napi_value element;
    napi_get_element(env, array, i, &element);
    out.push_back(JsStringToString(env, element));
  }
  return out;
}

napi_value FindClass(napi_env env, const char* name) {
  napi_value global;
  napi_status status = napi_get_global(env, &global);
  FML_DCHECK(status == napi_ok) << "napi_get_global failed";
  return GetNamedProperty(env, global, name);
}

napi_value NewInstance(napi_env env,
                       napi_value clazz,
                       size_t argc,
                       const napi_value* argv) {
  napi_value obj;
  napi_status status = napi_new_instance(env, clazz, argc, argv, &obj);
  FML_DCHECK(status == napi_ok) << "napi_new_instance failed";
  return obj;
}

napi_value NewObject(napi_env env,
                     const char* name,
                     size_t argc,
                     const napi_value* argv) {
  napi_value clazz = FindClass(env, name);
  return NewInstance(env, clazz, argc, argv);
}

napi_value GetNamedProperty(napi_env env, napi_value obj, const char* key) {
  if (!HasNamedProperty(env, obj, key)) {
    return nullptr;
  }
  napi_value result;
  napi_status status = napi_get_named_property(env, obj, key, &result);
  FML_DCHECK(status == napi_ok) << "napi_get_named_property failed";
  return result;
}

bool HasNamedProperty(napi_env env, napi_value obj, const char* key) {
  bool result;
  napi_status status = napi_has_named_property(env, obj, key, &result);
  return status == napi_ok && result;
}

void SetNamedProperty(napi_env env,
                      napi_value obj,
                      const char* key,
                      napi_value value) {
  if (!HasNamedProperty(env, obj, key)) {
    return;
  }
  napi_status status = napi_set_named_property(env, obj, key, value);
  FML_DCHECK(status == napi_ok) << "napi_set_named_property failed";
}

}  // namespace napi
}  // namespace fml
