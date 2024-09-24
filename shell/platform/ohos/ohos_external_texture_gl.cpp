/*
 * Copyright (c) 2023 Hunan OpenValley Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ohos_external_texture_gl.h"

#include <GLES2/gl2ext.h>

#include <utility>

#include "ohos_main.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace flutter {

OHOSExternalTextureGL::OHOSExternalTextureGL(int64_t id, OH_NativeImage* native_image)
  : Texture(id), native_image_(native_image), transform(SkMatrix::I())
{
  state_ = AttachmentState::uninitialized;
}

OHOSExternalTextureGL::~OHOSExternalTextureGL()
{
  FML_LOG(INFO) << "~OHOSExternalTextureGL, texture_name_=" << texture_name_ << ", Id()=" << Id();
  if (state_ == AttachmentState::attached) {
    glDeleteTextures(1, &texture_name_);
  }
}

void OHOSExternalTextureGL::Attach()
{
  FML_DLOG(INFO) << "OHOSExternalTextureGL::Attach, Id()=" << Id();
  int32_t ret = OH_NativeImage_AttachContext(native_image_, texture_name_);
  if (ret != 0) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL OH_NativeImage_AttachContext err code:" << ret;
  }
}

void OHOSExternalTextureGL::Paint(PaintContext& context,
                                  const SkRect& bounds,
                                  bool freeze,
                                  const SkSamplingOptions& sampling)
{
  if (state_ == AttachmentState::detached) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL::Paint, the current status is detached";
    return;
  }
  if (state_ == AttachmentState::uninitialized) {
    glGenTextures(1, &texture_name_);
    Attach();
    state_ = AttachmentState::attached;
  }
  if (!freeze && new_frame_ready_) {
    Update();
    new_frame_ready_ = false;
  }
  GrGLTextureInfo textureInfo = {GL_TEXTURE_EXTERNAL_OES, texture_name_, GL_RGBA8_OES};
  GrBackendTexture backendTexture(1, 1, GrMipMapped::kNo, textureInfo);
  sk_sp<SkImage> image = SkImage::MakeFromTexture(
      context.gr_context, backendTexture, kTopLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
  if (image) {
    SkAutoCanvasRestore autoRestore(context.canvas, true);

    // The incoming texture is vertically flipped, so we flip it
    // back. OpenGL's coordinate system has Positive Y equivalent to up, while
    // Skia's coordinate system has Negative Y equvalent to up.
    context.canvas->translate(bounds.x(), bounds.y() + bounds.height());
    context.canvas->scale(bounds.width(), -bounds.height());

    if (!transform.isIdentity()) {
      sk_sp<SkShader> shader = image->makeShader(
          SkTileMode::kRepeat, SkTileMode::kRepeat, sampling, transform);

      SkPaint paintWithShader;
      if (context.sk_paint) {
        paintWithShader = *context.sk_paint;
      }
      paintWithShader.setShader(shader);
      context.canvas->drawRect(SkRect::MakeWH(1, 1), paintWithShader);
    } else {
      context.canvas->drawImage(image, 0, 0, sampling, context.sk_paint);
    }
  }
}

void OHOSExternalTextureGL::OnGrContextCreated()
{
  FML_DLOG(INFO)<<" OHOSExternalTextureGL::OnGrContextCreated";
  state_ = AttachmentState::uninitialized;
}

void OHOSExternalTextureGL::OnGrContextDestroyed()
{
  FML_DLOG(INFO)<<" OHOSExternalTextureGL::OnGrContextDestroyed";
  if (state_ == AttachmentState::attached) {
    Detach();
    glDeleteTextures(1, &texture_name_);
  }
  state_ = AttachmentState::detached;
}

void OHOSExternalTextureGL::MarkNewFrameAvailable()
{
  FML_DLOG(INFO)<<" OHOSExternalTextureGL::MarkNewFrameAvailable";
  new_frame_ready_ = true;
}

void OHOSExternalTextureGL::OnTextureUnregistered()
{
  FML_DLOG(INFO) << " OHOSExternalTextureGL::OnTextureUnregistered, texture_name_=" << texture_name_
    << ", Id()=" << Id();
  if (native_image_ != nullptr) {
    OH_NativeImage_UnsetOnFrameAvailableListener(native_image_);
    OH_NativeImage_Destroy(&native_image_);
    native_image_ = nullptr;
  }
}

void OHOSExternalTextureGL::Update()
{
  FML_DLOG(INFO) << "OHOSExternalTextureGL::Update, texture_name_=" << texture_name_;
  if (native_image_ == nullptr) {
    FML_LOG(ERROR) << "Update, native_image is nullptr, texture_name_=" << texture_name_;
    return;
  }
  int32_t ret = OH_NativeImage_UpdateSurfaceImage(native_image_);
  if (ret != 0) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL OH_NativeImage_UpdateSurfaceImage err code:" << ret;
    return;
  }
  UpdateTransform();
}

void OHOSExternalTextureGL::Detach()
{
  FML_LOG(INFO) << "OHOSExternalTextureGL::Detach, texture_name_=" << texture_name_;
  OH_NativeImage_DetachContext(native_image_);
}

void OHOSExternalTextureGL::UpdateTransform()
{
  float m[16] = { 0.0f };
  int32_t ret = OH_NativeImage_GetTransformMatrixV2(native_image_, m);
  if (ret != 0) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL OH_NativeImage_GetTransformMatrixV2 err code:" << ret;
  }
  // transform ohos 4x4 matrix to skia 3x3 matrix
  SkScalar matrix3[] = {
    m[0], m[4], m[12],  //
    m[1], m[5], m[13],  //
    m[3], m[7], m[15],  //
  };
  transform.set9(matrix3);
  SkMatrix inverted;
  if (!transform.invert(&inverted)) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL Invalid SurfaceTexture transformation matrix";
  }
  transform = inverted;
}

void OHOSExternalTextureGL::setTextureBufferSize(int32_t width, int32_t height) {
  OHNativeWindow* native_window = OH_NativeImage_AcquireNativeWindow(native_image_);
  if (native_window == nullptr) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL::setTextureBufferSize native_window is nullptr";
    return;
  }
  int code = SET_BUFFER_GEOMETRY;
  int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(native_window, code, width, height);
  if (ret != 0) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL::setTextureBufferSize SET_BUFFER_GEMOTERY err:" << ret;
    return;
  }
  code = SET_TIMEOUT;
  ret = OH_NativeWindow_NativeWindowHandleOpt(native_window, code, 60);
  if (ret != 0) {
    FML_LOG(ERROR) << "OHOSExternalTextureGL::setTextureBufferSize SET_TIMEOUT OH_NativeWindow_NativeWindowHandleOpt err:" << ret;
    return;
  }
}

}  // namespace flutter