// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_OHOS_ENVIRONMENT_GL_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_OHOS_ENVIRONMENT_GL_H_

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_counted.h"

#include <EGL/egl.h>

namespace flutter {

class OHOSEnvironmentGL : public fml::RefCountedThreadSafe<OHOSEnvironmentGL> {
 private:
  // MakeRefCounted
  OHOSEnvironmentGL();

  // MakeRefCounted
  ~OHOSEnvironmentGL();

 public:
  bool IsValid() const;

  EGLDisplay Display() const;

 private:
  EGLDisplay display_;
  bool valid_ = false;

  FML_FRIEND_MAKE_REF_COUNTED(OHOSEnvironmentGL);
  FML_FRIEND_REF_COUNTED_THREAD_SAFE(OHOSEnvironmentGL);
  FML_DISALLOW_COPY_AND_ASSIGN(OHOSEnvironmentGL);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_OHOS_OHOS_ENVIRONMENT_GL_H_
