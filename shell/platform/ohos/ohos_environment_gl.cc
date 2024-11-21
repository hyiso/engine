// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_environment_gl.h"

namespace flutter {

OHOSEnvironmentGL::OHOSEnvironmentGL() : display_(EGL_NO_DISPLAY) {
  // Get the display.
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (display_ == EGL_NO_DISPLAY) {
    return;
  }

  // Initialize the display connection.
  if (eglInitialize(display_, nullptr, nullptr) != EGL_TRUE) {
    return;
  }

  valid_ = true;
}

OHOSEnvironmentGL::~OHOSEnvironmentGL() {
  // Disconnect the display if valid.
  if (display_ != EGL_NO_DISPLAY) {
    eglTerminate(display_);
  }
}

bool OHOSEnvironmentGL::IsValid() const {
  return valid_;
}

EGLDisplay OHOSEnvironmentGL::Display() const {
  return display_;
}

}  // namespace flutter
