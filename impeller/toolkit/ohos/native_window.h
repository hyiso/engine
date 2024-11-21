// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_TOOLKIT_OHOS_NATIVE_WINDOW_H_
#define FLUTTER_IMPELLER_TOOLKIT_OHOS_NATIVE_WINDOW_H_

#include <native_window/external_window.h>

#include "flutter/fml/unique_object.h"
#include "impeller/geometry/size.h"

namespace impeller::ohos {

//------------------------------------------------------------------------------
/// @brief      A wrapper for OHNativeWindow
///             https://developer.huawei.com/consumer/en/doc/harmonyos-references-V5/_native_window-V5
///
///             This wrapper is only available on OHOS.
///
class NativeWindow {
 public:
  explicit NativeWindow(OHNativeWindow* window);

  ~NativeWindow();

  NativeWindow(const NativeWindow&) = delete;

  NativeWindow& operator=(const NativeWindow&) = delete;

  bool IsValid() const;

  //----------------------------------------------------------------------------
  /// @return     The current size of the native window.
  ///
  ISize GetSize() const;

  OHNativeWindow* GetHandle() const;

 private:
  struct UniqueOHNativeWindowTraits {
    static OHNativeWindow* InvalidValue() { return nullptr; }

    static bool IsValid(OHNativeWindow* value) {
      return value != InvalidValue();
    }

    static void Free(OHNativeWindow* value) {
      OH_NativeWindow_DestroyNativeWindow(value);
    }
  };

  fml::UniqueObject<OHNativeWindow*, UniqueOHNativeWindowTraits> window_;
};

}  // namespace impeller::ohos

#endif  // FLUTTER_IMPELLER_TOOLKIT_OHOS_NATIVE_WINDOW_H_
