// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/ohos/ohb_texture_source_vk.h"

#include "impeller/renderer/backend/vulkan/allocator_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/yuv_conversion_library_vk.h"

namespace impeller {

using OHBProperties = vk::StructureChain<
    // For VK_OHOS_external_memory
    vk::NativeBufferPropertiesOHOS,
    // For VK_OHOS_external_memory
    vk::NativeBufferFormatPropertiesOHOS>;

static vk::UniqueImage CreateVKImageWrapperForNativeBufferOHOS(
    const vk::Device& device,
    const OHBProperties& ohb_props,
    const OH_NativeBuffer_Config& oh_config) {
  const auto& ohb_format =
      ohb_props.get<vk::NativeBufferFormatPropertiesOHOS>();

  vk::StructureChain<vk::ImageCreateInfo,
                     // For VK_KHR_external_memory
                     vk::ExternalMemoryImageCreateInfo,
                     // For VK_OHOS_external_memory
                     vk::ExternalFormatOHOS>
      image_chain;

  auto& image_info = image_chain.get<vk::ImageCreateInfo>();

  vk::ImageUsageFlags image_usage_flags = vk::ImageUsageFlagBits::eSampled;
  if (oh_config.usage & NATIVEBUFFER_USAGE_HW_RENDER) {
    image_usage_flags |= vk::ImageUsageFlagBits::eColorAttachment;
  }

  vk::ImageCreateFlags image_create_flags = vk::ImageCreateFlags(0);

  image_info.imageType = vk::ImageType::e2D;
  image_info.format = ohb_format.format;
  image_info.extent.width = oh_config.width;
  image_info.extent.height = oh_config.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1u;
  image_info.arrayLayers = 1u;
  image_info.samples = vk::SampleCountFlagBits::e1;
  image_info.tiling = vk::ImageTiling::eOptimal;
  image_info.usage = image_usage_flags;
  image_info.flags = image_create_flags;
  image_info.sharingMode = vk::SharingMode::eExclusive;
  image_info.initialLayout = vk::ImageLayout::eUndefined;

  image_chain.get<vk::ExternalMemoryImageCreateInfo>().handleTypes =
      vk::ExternalMemoryHandleTypeFlagBits::eNativeBufferOHOS;

  // If the format isn't natively supported by Vulkan (i.e, be a part of the
  // base vkFormat enum), an untyped "external format" must be specified when
  // creating the image and the image views. Usually includes YUV formats.
  if (ohb_format.format == vk::Format::eUndefined) {
    image_chain.get<vk::ExternalFormatOHOS>().externalFormat =
        ohb_format.externalFormat;
  } else {
    image_chain.unlink<vk::ExternalFormatOHOS>();
  }

  auto image = device.createImageUnique(image_chain.get());
  if (image.result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not create image for external buffer: "
                   << vk::to_string(image.result);
    return {};
  }

  return std::move(image.value);
}

static vk::UniqueDeviceMemory ImportVKDeviceMemoryFromOHNativeBuffer(
    const vk::Device& device,
    const vk::PhysicalDevice& physical_device,
    const vk::Image& image,
    struct OH_NativeBuffer* native_buffer,
    const OHBProperties& ohb_props) {
  vk::PhysicalDeviceMemoryProperties memory_properties;
  physical_device.getMemoryProperties(&memory_properties);
  int memory_type_index = AllocatorVK::FindMemoryTypeIndex(
      ohb_props.get().memoryTypeBits, memory_properties);
  if (memory_type_index < 0) {
    VALIDATION_LOG << "Could not find memory type of external image.";
    return {};
  }

  vk::StructureChain<vk::MemoryAllocateInfo,
                     // Core in 1.1
                     vk::MemoryDedicatedAllocateInfo,
                     // For VK_OHOS_external_memory
                     vk::ImportNativeBufferInfoOHOS>
      memory_chain;

  auto& mem_alloc_info = memory_chain.get<vk::MemoryAllocateInfo>();
  mem_alloc_info.allocationSize = ohb_props.get().allocationSize;
  mem_alloc_info.memoryTypeIndex = memory_type_index;

  auto& dedicated_alloc_info =
      memory_chain.get<vk::MemoryDedicatedAllocateInfo>();
  dedicated_alloc_info.image = image;

  auto& ohb_import_info = memory_chain.get<vk::ImportNativeBufferInfoOHOS>();
  ohb_import_info.buffer = native_buffer;

  auto device_memory = device.allocateMemoryUnique(memory_chain.get());
  if (device_memory.result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not allocate device memory for external image : "
                   << vk::to_string(device_memory.result);
    return {};
  }

  return std::move(device_memory.value);
}

static std::shared_ptr<YUVConversionVK> CreateYUVConversion(
    const ContextVK& context,
    const OHBProperties& ohb_props) {
  YUVConversionDescriptorVK conversion_chain;

  const auto& ohb_format =
      ohb_props.get<vk::NativeBufferFormatPropertiesOHOS>();

  auto& conversion_info = conversion_chain.get();

  conversion_info.format = ohb_format.format;
  conversion_info.ycbcrModel = ohb_format.suggestedYcbcrModel;
  conversion_info.ycbcrRange = ohb_format.suggestedYcbcrRange;
  conversion_info.components = ohb_format.samplerYcbcrConversionComponents;
  conversion_info.xChromaOffset = ohb_format.suggestedXChromaOffset;
  conversion_info.yChromaOffset = ohb_format.suggestedYChromaOffset;
  // If the potential format features of the sampler Y′CBCR conversion do not
  // support VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT,
  // chromaFilter must not be VK_FILTER_LINEAR.
  //
  // Since we are not checking, let's just default to a safe value.
  conversion_info.chromaFilter = vk::Filter::eNearest;
  conversion_info.forceExplicitReconstruction = false;

  if (conversion_info.format == vk::Format::eUndefined) {
    auto& external_format = conversion_chain.get<vk::ExternalFormatOHOS>();
    external_format.externalFormat = ohb_format.externalFormat;
  } else {
    conversion_chain.unlink<vk::ExternalFormatOHOS>();
  }

  return context.GetYUVConversionLibrary()->GetConversion(conversion_chain);
}

static vk::UniqueImageView CreateVKImageView(
    const vk::Device& device,
    const vk::Image& image,
    const vk::SamplerYcbcrConversion& yuv_conversion,
    const OHBProperties& ohb_props,
    const OH_NativeBuffer_Config& oh_config) {
  const auto& ohb_format =
      ohb_props.get<vk::NativeBufferFormatPropertiesOHOS>();

  vk::StructureChain<vk::ImageViewCreateInfo,
                     // Core in 1.1
                     vk::SamplerYcbcrConversionInfo>
      view_chain;

  auto& view_info = view_chain.get();

  view_info.image = image;
  view_info.viewType = vk::ImageViewType::e2D;
  view_info.format = ohb_format.format;
  view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  view_info.subresourceRange.baseMipLevel = 0u;
  view_info.subresourceRange.baseArrayLayer = 0u;
  view_info.subresourceRange.levelCount = 1u;
  view_info.subresourceRange.layerCount = 1u;

  // We need a custom YUV conversion only if we don't recognize the format.
  if (view_info.format == vk::Format::eUndefined) {
    view_chain.get<vk::SamplerYcbcrConversionInfo>().conversion =
        yuv_conversion;
  } else {
    view_chain.unlink<vk::SamplerYcbcrConversionInfo>();
  }

  auto image_view = device.createImageViewUnique(view_info);
  if (image_view.result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not create external image view: "
                   << vk::to_string(image_view.result);
    return {};
  }

  return std::move(image_view.value);
}

static PixelFormat ToPixelFormat(int32_t format) {
  switch (format) {
    case NATIVEBUFFER_PIXEL_FMT_RGBA_8888:
      return PixelFormat::kR8G8B8A8UNormInt;
    case NATIVEBUFFER_PIXEL_FMT_BGRA_8888:
      return PixelFormat::kB8G8R8A8UNormInt;
      // Not understood by the rest of Impeller. Use a placeholder but create
      // the native image and image views using the right external format.
      break;
  }
  return PixelFormat::kR8G8B8A8UNormInt;
}

static TextureDescriptor ToTextureDescriptor(
    const OH_NativeBuffer_Config& oh_config) {
  const auto ohb_size = ISize{oh_config.width, oh_config.height};
  TextureDescriptor desc;
  // We are not going to touch hardware buffers on the CPU or use them as
  // transient attachments. Just treat them as device private.
  desc.storage_mode = StorageMode::kDevicePrivate;
  desc.format = ToPixelFormat(oh_config.format);
  desc.size = ohb_size;
  desc.type = TextureType::kTexture2D;
  desc.sample_count = SampleCount::kCount1;
  desc.compression_type = CompressionType::kLossless;
  desc.mip_count = 1u;
  return desc;
}

OHBTextureSourceVK::OHBTextureSourceVK(
    const std::shared_ptr<Context>& p_context,
    struct OH_NativeBuffer* native_buffer,
    const OH_NativeBuffer_Config& oh_config)
    : TextureSourceVK(ToTextureDescriptor(oh_config)) {
  if (!p_context) {
    return;
  }

  const auto& context = ContextVK::Cast(*p_context);

  const auto& device = context.GetDevice();
  const auto& physical_device = context.GetPhysicalDevice();

  OHBProperties ohb_props;

  if (device.getNativeBufferPropertiesOHOS(native_buffer, &ohb_props.get()) !=
      vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not determine properties of the Android hardware "
                      "buffer.";
    return;
  }

  const auto& ohb_format =
      ohb_props.get<vk::NativeBufferFormatPropertiesOHOS>();

  // Create an image to refer to our external image.
  auto image =
      CreateVKImageWrapperForNativeBufferOHOS(device, ohb_props, oh_config);
  if (!image) {
    return;
  }

  // Create a device memory allocation to refer to our external image.
  auto device_memory = ImportVKDeviceMemoryFromOHNativeBuffer(
      device, physical_device, image.get(), native_buffer, ohb_props);
  if (!device_memory) {
    return;
  }

  // Bind the image to the image memory.
  if (auto result = device.bindImageMemory(image.get(), device_memory.get(), 0);
      result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not bind external device memory to image : "
                   << vk::to_string(result);
    return;
  }

  // Figure out how to perform YUV conversions.
  auto yuv_conversion = CreateYUVConversion(context, ohb_props);
  if (!yuv_conversion || !yuv_conversion->IsValid()) {
    return;
  }

  // Create image view for the newly created image.
  auto image_view = CreateVKImageView(device,                           //
                                      image.get(),                      //
                                      yuv_conversion->GetConversion(),  //
                                      ohb_props,                        //
                                      oh_config                         //
  );
  if (!image_view) {
    return;
  }

  needs_yuv_conversion_ = ohb_format.format == vk::Format::eUndefined;
  device_memory_ = std::move(device_memory);
  image_ = std::move(image);
  yuv_conversion_ = std::move(yuv_conversion);
  image_view_ = std::move(image_view);

#ifdef IMPELLER_DEBUG
  context.SetDebugName(device_memory_.get(), "OHB Device Memory");
  context.SetDebugName(image_.get(), "OHB Image");
  context.SetDebugName(yuv_conversion_->GetConversion(), "OHB YUV Conversion");
  context.SetDebugName(image_view_.get(), "OHB ImageView");
#endif  // IMPELLER_DEBUG

  is_valid_ = true;
}

OHBTextureSourceVK::OHBTextureSourceVK(
    const std::shared_ptr<Context>& context,
    std::unique_ptr<ohos::NativeBuffer> backing_store,
    bool is_swapchain_image)
    : OHBTextureSourceVK(context,
                         backing_store->GetHandle(),
                         backing_store->GetOHOSConfig()) {
  backing_store_ = std::move(backing_store);
  is_swapchain_image_ = is_swapchain_image;
}

// |TextureSourceVK|
OHBTextureSourceVK::~OHBTextureSourceVK() = default;

bool OHBTextureSourceVK::IsValid() const {
  return is_valid_;
}

// |TextureSourceVK|
vk::Image OHBTextureSourceVK::GetImage() const {
  return image_.get();
}

// |TextureSourceVK|
vk::ImageView OHBTextureSourceVK::GetImageView() const {
  return image_view_.get();
}

// |TextureSourceVK|
vk::ImageView OHBTextureSourceVK::GetRenderTargetView() const {
  return image_view_.get();
}

// |TextureSourceVK|
bool OHBTextureSourceVK::IsSwapchainImage() const {
  return is_swapchain_image_;
}

// |TextureSourceVK|
std::shared_ptr<YUVConversionVK> OHBTextureSourceVK::GetYUVConversion() const {
  return needs_yuv_conversion_ ? yuv_conversion_ : nullptr;
}

const ohos::NativeBuffer* OHBTextureSourceVK::GetBackingStore() const {
  return backing_store_.get();
}

}  // namespace impeller
