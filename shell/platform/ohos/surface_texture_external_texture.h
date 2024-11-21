// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_SURFACE_TEXTURE_EXTERNAL_TEXTURE_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_SURFACE_TEXTURE_EXTERNAL_TEXTURE_H_

#include <GLES3/gl3.h>
#include <native_image/native_image.h>
#include <native_window/external_window.h>

#include "flutter/common/graphics/texture.h"
#include "flutter/shell/platform/ohos/platform_view_ohos_napi_impl.h"

namespace flutter {

class SurfaceTextureExternalTexture : public flutter::Texture {
 public:
  SurfaceTextureExternalTexture(
      int64_t id,
      OH_NativeImage* native_image,
      const std::shared_ptr<PlatformViewOHOSNAPI>& napi_facade);

  ~SurfaceTextureExternalTexture() override;

  void Paint(PaintContext& context,
             const SkRect& bounds,
             bool freeze,
             const DlImageSampling sampling) override;

  void OnGrContextCreated() override;

  void OnGrContextDestroyed() override;

  void MarkNewFrameAvailable() override;

  void OnTextureUnregistered() override;

  void setTextureBufferSize(int32_t width, int32_t height);

 protected:
  virtual void ProcessFrame(PaintContext& context, const SkRect& bounds) = 0;
  virtual void Detach();

  void Attach(int gl_tex_id);
  bool ShouldUpdate();
  void Update();

  enum class AttachmentState { kUninitialized, kAttached, kDetached };

  std::shared_ptr<PlatformViewOHOSNAPI> napi_facade_;
  OH_NativeImage* native_image_;
  bool new_frame_available_ = false;
  AttachmentState state_ = AttachmentState::kUninitialized;
  SkMatrix transform_;
  sk_sp<flutter::DlImage> dl_image_;

  FML_DISALLOW_COPY_AND_ASSIGN(SurfaceTextureExternalTexture);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_OHOS_SURFACE_TEXTURE_EXTERNAL_TEXTURE_H_
