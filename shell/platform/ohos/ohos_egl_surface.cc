// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_egl_surface.h"

#include <EGL/eglext.h>

#include <array>
#include <list>

#include "flutter/fml/trace_event.h"

namespace flutter {

void LogLastEGLError() {
  struct EGLNameErrorPair {
    const char* name;
    EGLint code;
  };

#define _EGL_ERROR_DESC(a) \
  { #a, a }

  const EGLNameErrorPair pairs[] = {
      _EGL_ERROR_DESC(EGL_SUCCESS),
      _EGL_ERROR_DESC(EGL_NOT_INITIALIZED),
      _EGL_ERROR_DESC(EGL_BAD_ACCESS),
      _EGL_ERROR_DESC(EGL_BAD_ALLOC),
      _EGL_ERROR_DESC(EGL_BAD_ATTRIBUTE),
      _EGL_ERROR_DESC(EGL_BAD_CONTEXT),
      _EGL_ERROR_DESC(EGL_BAD_CONFIG),
      _EGL_ERROR_DESC(EGL_BAD_CURRENT_SURFACE),
      _EGL_ERROR_DESC(EGL_BAD_DISPLAY),
      _EGL_ERROR_DESC(EGL_BAD_SURFACE),
      _EGL_ERROR_DESC(EGL_BAD_MATCH),
      _EGL_ERROR_DESC(EGL_BAD_PARAMETER),
      _EGL_ERROR_DESC(EGL_BAD_NATIVE_PIXMAP),
      _EGL_ERROR_DESC(EGL_BAD_NATIVE_WINDOW),
      _EGL_ERROR_DESC(EGL_CONTEXT_LOST),
  };

#undef _EGL_ERROR_DESC

  const auto count = sizeof(pairs) / sizeof(EGLNameErrorPair);

  EGLint last_error = eglGetError();

  for (size_t i = 0; i < count; i++) {
    if (last_error == pairs[i].code) {
      FML_LOG(ERROR) << "EGL Error: " << pairs[i].name << " (" << pairs[i].code
                     << ")";
      return;
    }
  }

  FML_LOG(ERROR) << "Unknown EGL Error";
}

class OHOSEGLSurfaceDamage {
 public:
  void init(EGLDisplay display, EGLContext context) {}

  void SetDamageRegion(EGLDisplay display,
                       EGLSurface surface,
                       const std::optional<SkIRect>& region) {}

  /// This was disabled after discussion in
  /// https://github.com/flutter/flutter/issues/123353
  bool SupportsPartialRepaint() const { return false; }

  std::optional<SkIRect> InitialDamage(EGLDisplay display, EGLSurface surface) {
    return std::nullopt;
  }

  bool SwapBuffersWithDamage(EGLDisplay display,
                             EGLSurface surface,
                             const std::optional<SkIRect>& damage) {
    return eglSwapBuffers(display, surface);
  }
};

OHOSEGLSurface::OHOSEGLSurface(EGLSurface surface,
                               EGLDisplay display,
                               EGLContext context)
    : surface_(surface),
      display_(display),
      context_(context),
      damage_(std::make_unique<OHOSEGLSurfaceDamage>()) {
  damage_->init(display_, context);
}

OHOSEGLSurface::~OHOSEGLSurface() {
  [[maybe_unused]] auto result = eglDestroySurface(display_, surface_);
  FML_DCHECK(result == EGL_TRUE);
}

bool OHOSEGLSurface::IsValid() const {
  return surface_ != EGL_NO_SURFACE;
}

bool OHOSEGLSurface::IsContextCurrent() const {
  EGLContext current_egl_context = eglGetCurrentContext();
  if (context_ != current_egl_context) {
    return false;
  }

  EGLDisplay current_egl_display = eglGetCurrentDisplay();
  if (display_ != current_egl_display) {
    return false;
  }

  EGLSurface draw_surface = eglGetCurrentSurface(EGL_DRAW);
  if (draw_surface != surface_) {
    return false;
  }

  EGLSurface read_surface = eglGetCurrentSurface(EGL_READ);
  if (read_surface != surface_) {
    return false;
  }

  return true;
}

OHOSEGLSurfaceMakeCurrentStatus OHOSEGLSurface::MakeCurrent() const {
  if (IsContextCurrent()) {
    return OHOSEGLSurfaceMakeCurrentStatus::kSuccessAlreadyCurrent;
  }
  if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
    FML_LOG(ERROR) << "Could not make the context current";
    LogLastEGLError();
    return OHOSEGLSurfaceMakeCurrentStatus::kFailure;
  }
  return OHOSEGLSurfaceMakeCurrentStatus::kSuccessMadeCurrent;
}

void OHOSEGLSurface::SetDamageRegion(
    const std::optional<SkIRect>& buffer_damage) {
  damage_->SetDamageRegion(display_, surface_, buffer_damage);
}

bool OHOSEGLSurface::SetPresentationTime(
    const fml::TimePoint& presentation_time) {
  if (presentation_time_proc_) {
    const auto time_ns = presentation_time.ToEpochDelta().ToNanoseconds();
    return presentation_time_proc_(display_, surface_, time_ns);
  } else {
    return false;
  }
}

bool OHOSEGLSurface::SwapBuffers(const std::optional<SkIRect>& surface_damage) {
  TRACE_EVENT0("flutter", "OHOSContextGL::SwapBuffers");
  return damage_->SwapBuffersWithDamage(display_, surface_, surface_damage);
}

bool OHOSEGLSurface::SupportsPartialRepaint() const {
  return damage_->SupportsPartialRepaint();
}

std::optional<SkIRect> OHOSEGLSurface::InitialDamage() {
  return damage_->InitialDamage(display_, surface_);
}

SkISize OHOSEGLSurface::GetSize() const {
  EGLint width = 0;
  EGLint height = 0;

  if (!eglQuerySurface(display_, surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(display_, surface_, EGL_HEIGHT, &height)) {
    FML_LOG(ERROR) << "Unable to query EGL surface size";
    LogLastEGLError();
    return SkISize::Make(0, 0);
  }
  return SkISize::Make(width, height);
}

}  // namespace flutter
