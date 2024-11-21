// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_image_generator.h"

#include <memory>
#include <utility>

#include <multimedia/image_framework/image/image_source_native.h>
#include <multimedia/image_framework/image/pixelmap_native.h>

#include "third_party/skia/include/codec/SkCodecAnimation.h"

namespace flutter {

OHOSImageGenerator::OHOSImageGenerator(sk_sp<SkData> data)
    : data_(std::move(data)), image_info_(SkImageInfo::MakeUnknown(-1, -1)) {}

OHOSImageGenerator::~OHOSImageGenerator() = default;

const SkImageInfo& OHOSImageGenerator::GetInfo() {
  header_decoded_latch_.Wait();
  return image_info_;
}

unsigned int OHOSImageGenerator::GetFrameCount() const {
  return 1;
}

unsigned int OHOSImageGenerator::GetPlayCount() const {
  return 1;
}

const ImageGenerator::FrameInfo OHOSImageGenerator::GetFrameInfo(
    unsigned int frame_index) {
  return {.required_frame = std::nullopt,
          .duration = 0,
          .disposal_method = SkCodecAnimation::DisposalMethod::kKeep};
}

SkISize OHOSImageGenerator::GetScaledDimensions(float desired_scale) {
  return GetInfo().dimensions();
}

// |ImageGenerator|
bool OHOSImageGenerator::GetPixels(const SkImageInfo& info,
                                   void* pixels,
                                   size_t row_bytes,
                                   unsigned int frame_index,
                                   std::optional<unsigned int> prior_frame) {
  fully_decoded_latch_.Wait();

  if (!software_decoded_data_) {
    return false;
  }

  if (kRGBA_8888_SkColorType != info.colorType()) {
    return false;
  }

  switch (info.alphaType()) {
    case kOpaque_SkAlphaType:
      if (kOpaque_SkAlphaType != GetInfo().alphaType()) {
        return false;
      }
      break;
    case kPremul_SkAlphaType:
      break;
    default:
      return false;
  }

  // TODO(bdero): Override `GetImage()` to use `SkImage::FromAHardwareBuffer` on
  // API level 30+ once it's updated to do symbol lookups and not get
  // preprocessed out in Skia. This will allow for avoiding this copy in
  // cases where the result image doesn't need to be resized.
  memcpy(pixels, software_decoded_data_->data(),
         software_decoded_data_->size());
  return true;
}

void OHOSImageGenerator::DecodeImage() {
  DoDecodeImage();

  header_decoded_latch_.Signal();
  fully_decoded_latch_.Signal();
}

void OHOSImageGenerator::DoDecodeImage() {
  OH_ImageSourceNative* image_source = nullptr;
  OH_ImageSourceNative_CreateFromData((uint8_t*)data_->bytes(), data_->size(),
                                      &image_source);
  OH_ImageSource_Info* info = nullptr;
  OH_ImageSourceInfo_Create(&info);
  OH_ImageSourceNative_GetImageInfo(image_source, 0, info);
  uint32_t width = 0, height = 0;
  OH_ImageSourceInfo_GetWidth(info, &width);
  OH_ImageSourceInfo_GetHeight(info, &height);
  image_info_ = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                  kOpaque_SkAlphaType);

  OH_DecodingOptions* options = nullptr;
  OH_DecodingOptions_Create(&options);
  Image_Size size = {width, height};
  OH_DecodingOptions_SetDesiredSize(options, &size);
  OH_DecodingOptions_SetPixelFormat(options, PIXEL_FORMAT_RGBA_8888);
  OH_DecodingOptions_SetDesiredDynamicRange(options, IMAGE_DYNAMIC_RANGE_HDR);
  OH_DecodingOptions_SetIndex(options, 0);

  OH_PixelmapNative* pixel_map = nullptr;
  OH_ImageSourceNative_CreatePixelmap(image_source, options, &pixel_map);

  size_t data_size = width * height * sizeof(uint32_t);
  uint8_t* data_buffer = new uint8_t[data_size];
  OH_PixelmapNative_ReadPixels(pixel_map, data_buffer, &data_size);
  software_decoded_data_ = SkData::MakeWithCopy(data_buffer, data_size);

  delete[] data_buffer;
  OH_ImageSourceNative_Release(image_source);
  OH_ImageSourceInfo_Release(info);
  OH_DecodingOptions_Release(options);
  OH_PixelmapNative_Release(pixel_map);
}

std::shared_ptr<ImageGenerator> OHOSImageGenerator::MakeFromData(
    sk_sp<SkData> data,
    const fml::RefPtr<fml::TaskRunner>& task_runner) {
  std::shared_ptr<OHOSImageGenerator> generator(
      new OHOSImageGenerator(std::move(data)));

  fml::TaskRunner::RunNowOrPostTask(
      task_runner, [generator]() { generator->DecodeImage(); });

  if (generator->IsValidImageData()) {
    return generator;
  }
  return nullptr;
}

bool OHOSImageGenerator::IsValidImageData() {
  // The generator kicks off an IO task to decode everything, and calls to
  // "GetInfo()" block until either the header has been decoded or decoding has
  // failed, whichever is sooner. The decoder is initialized with a width and
  // height of -1 and will update the dimensions if the image is able to be
  // decoded.
  return GetInfo().height() != -1;
}

}  // namespace flutter