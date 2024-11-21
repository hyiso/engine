// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/surface_texture_external_texture.h"

#include <utility>

#include "flutter/display_list/effects/dl_color_source.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace flutter {

static void OnNativeImageFrameAvailable(void* data) {
  auto texture = reinterpret_cast<SurfaceTextureExternalTexture*>(data);
  texture->MarkNewFrameAvailable();
}

SurfaceTextureExternalTexture::SurfaceTextureExternalTexture(
    int64_t id,
    OH_NativeImage* native_image,
    const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade)
    : Texture(id),
      napi_facade_(napi_facade),
      native_image_(native_image),
      transform_(SkMatrix::I()) {
  OH_OnFrameAvailableListener listener;
  listener.context = this;
  listener.onFrameAvailable = &OnNativeImageFrameAvailable;
  int32_t ret =
      OH_NativeImage_SetOnFrameAvailableListener(native_image, listener);
  if (ret != 0) {
    FML_LOG(ERROR) << "OH_NativeImage_SetOnFrameAvailableListener err:" << ret;
  }
}

SurfaceTextureExternalTexture::~SurfaceTextureExternalTexture() {}

void SurfaceTextureExternalTexture::OnGrContextCreated() {
  state_ = AttachmentState::kUninitialized;
}

void SurfaceTextureExternalTexture::MarkNewFrameAvailable() {
  new_frame_available_ = true;
}

void SurfaceTextureExternalTexture::Paint(PaintContext& context,
                                          const SkRect& bounds,
                                          bool freeze,
                                          const DlImageSampling sampling) {
  if (state_ == AttachmentState::kDetached) {
    return;
  }
  const bool should_process_frame =
      !freeze || ShouldUpdate() || dl_image_ == nullptr;
  if (should_process_frame) {
    ProcessFrame(context, bounds);
  }
  FML_CHECK(state_ == AttachmentState::kAttached);

  if (dl_image_) {
    DlAutoCanvasRestore autoRestore(context.canvas, true);

    // The incoming texture is vertically flipped, so we flip it
    // back. OpenGL's coordinate system has Positive Y equivalent to up, while
    // Skia's coordinate system has Negative Y equvalent to up.
    context.canvas->Translate(bounds.x(), bounds.y() + bounds.height());
    context.canvas->Scale(bounds.width(), -bounds.height());

    if (!transform_.isIdentity()) {
      DlImageColorSource source(dl_image_, DlTileMode::kClamp,
                                DlTileMode::kClamp, sampling, &transform_);

      DlPaint paintWithShader;
      if (context.paint) {
        paintWithShader = *context.paint;
      }
      paintWithShader.setColorSource(&source);
      context.canvas->DrawRect(SkRect::MakeWH(1, 1), paintWithShader);
    } else {
      context.canvas->DrawImage(dl_image_, {0, 0}, sampling, context.paint);
    }
  } else {
    FML_LOG(WARNING)
        << "No DlImage available for SurfaceTextureExternalTexture to paint.";
  }
}

void SurfaceTextureExternalTexture::OnGrContextDestroyed() {
  if (state_ == AttachmentState::kAttached) {
    Detach();
  }
  state_ = AttachmentState::kDetached;
}

void SurfaceTextureExternalTexture::OnTextureUnregistered() {
  if (native_image_ != nullptr) {
    OH_NativeImage_UnsetOnFrameAvailableListener(native_image_);
    OH_NativeImage_Destroy(&native_image_);
    native_image_ = nullptr;
  }
}

void SurfaceTextureExternalTexture::Detach() {
  OH_NativeImage_DetachContext(native_image_);
  dl_image_.reset();
}

void SurfaceTextureExternalTexture::Attach(int gl_tex_id) {
  int32_t ret = OH_NativeImage_AttachContext(native_image_, gl_tex_id);
  FML_DCHECK(ret == 0) << "OH_NativeImage_AttachContext err:" << ret;
  state_ = AttachmentState::kAttached;
}

bool SurfaceTextureExternalTexture::ShouldUpdate() {
  return new_frame_available_;
}

void SurfaceTextureExternalTexture::Update() {
  if (new_frame_available_ == false) {
    return;
  }
  new_frame_available_ = false;
  int32_t ret = OH_NativeImage_UpdateSurfaceImage(native_image_);
  if (ret != 0) {
    FML_DLOG(ERROR) << "OH_NativeImage_UpdateSurfaceImage err:" << ret;
    return;
  }

  float m[16] = {0.0f};
  ret = OH_NativeImage_GetTransformMatrixV2(native_image_, m);
  if (ret != 0) {
    FML_DLOG(ERROR) << "OH_NativeImage_GetTransformMatrixV2 err:" << ret;
    return;
  }

  // SurfaceTexture 4x4 Column Major -> Skia 3x3 Row Major

  // SurfaceTexture 4x4 (Column Major):
  // | m[0] m[4] m[ 8] m[12] |
  // | m[1] m[5] m[ 9] m[13] |
  // | m[2] m[6] m[10] m[14] |
  // | m[3] m[7] m[11] m[15] |

  // According to HarmonyOS documentation, the 4x4 matrix returned should be
  // used with texture coordinates in the form (s, t, 0, 1). Since the z
  // component is always 0.0, we are free to ignore any element that multiplies
  // with the z component. Converting this to a 3x3 matrix is easy:

  // SurfaceTexture 3x3 (Column Major):
  // | m[0] m[4] m[12] |
  // | m[1] m[5] m[13] |
  // | m[3] m[7] m[15] |

  // Skia (Row Major):
  // | m[0] m[1] m[2] |
  // | m[3] m[4] m[5] |
  // | m[6] m[7] m[8] |

  SkScalar matrix3[] = {
      m[0], m[4], m[12],  //
      m[1], m[5], m[13],  //
      m[3], m[7], m[15],  //
  };
  transform_.set9(matrix3);

  // OHOS's SurfaceTexture transform matrix works on texture coordinate
  // lookups in the range 0.0-1.0, while Skia's Shader transform matrix works on
  // the image itself, as if it were inscribed inside a clip rect.
  // An OHOS transform that scales lookup by 0.5 (displaying 50% of the
  // texture) is the same as a Skia transform by 2.0 (scaling 50% of the image
  // outside of the virtual "clip rect"), so we invert the incoming matrix.
  SkMatrix inverted;
  if (!transform_.invert(&inverted)) {
    FML_LOG(FATAL)
        << "Invalid (not invertable) SurfaceTexture transformation matrix";
  }
  transform_ = inverted;
}

void SurfaceTextureExternalTexture::setTextureBufferSize(int32_t width,
                                                         int32_t height) {
  OHNativeWindow* native_window =
      OH_NativeImage_AcquireNativeWindow(native_image_);
  if (native_window == nullptr) {
    FML_DLOG(ERROR) << "OHOSExternalTextureGL::setTextureBufferSize "
                       "native_window is nullptr";
    return;
  }
  int code = SET_BUFFER_GEOMETRY;
  int32_t ret =
      OH_NativeWindow_NativeWindowHandleOpt(native_window, code, width, height);
  if (ret != 0) {
    FML_DLOG(ERROR) << "OH_NativeWindow_NativeWindowHandleOpt err:" << ret;
    return;
  }
}

}  // namespace flutter
