// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/ohos/ohos_asset_provider.h"
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

namespace flutter {

class OHOSAssetMapping : public fml::Mapping {
 public:
  explicit OHOSAssetMapping(RawFile* asset) : asset_(asset) {}

  ~OHOSAssetMapping() override {
    if (asset_ != nullptr) {
      OH_ResourceManager_CloseRawFile(asset_);
    }
  }

  size_t GetSize() const override {
    if (asset_ == nullptr) {
      return 0;
    }
    return OH_ResourceManager_GetRawFileSize(asset_);
  }

  const uint8_t* GetMapping() const override {
    size_t len = GetSize();
    void* buffer = malloc(len);
    OH_ResourceManager_ReadRawFile(asset_, buffer, len);
    return static_cast<const uint8_t*>(buffer);
  }

  bool IsDontNeedSafe() const override { return false; }

 private:
  RawFile* const asset_;
  FML_DISALLOW_COPY_AND_ASSIGN(OHOSAssetMapping);
};

class OHOSAssetProviderImpl : public OHOSAssetProviderInternal {
 public:
  explicit OHOSAssetProviderImpl(NativeResourceManager* assetManager,
                                 std::string directory)
      : asset_manager_(assetManager), directory_(std::move(directory)) {}

  ~OHOSAssetProviderImpl() = default;

  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const override {
    std::stringstream ss;
    ss << directory_.c_str() << "/" << asset_name;
    RawFile* asset =
        OH_ResourceManager_OpenRawFile(asset_manager_, ss.str().c_str());
    if (!asset) {
      return nullptr;
    }

    return std::make_unique<OHOSAssetMapping>(asset);
  };

 private:
  NativeResourceManager* asset_manager_;
  const std::string directory_;

  FML_DISALLOW_COPY_AND_ASSIGN(OHOSAssetProviderImpl);
};

OHOSAssetProvider::OHOSAssetProvider(NativeResourceManager* assetManager,
                                     const std::string directory)
    : impl_(std::make_shared<OHOSAssetProviderImpl>(assetManager,
                                                    std::move(directory))) {}

OHOSAssetProvider::OHOSAssetProvider(
    std::shared_ptr<OHOSAssetProviderInternal> impl)
    : impl_(std::move(impl)) {}

bool OHOSAssetProvider::IsValid() const {
  return true;
}

bool OHOSAssetProvider::IsValidAfterAssetManagerChange() const {
  return true;
}

AssetResolver::AssetResolverType OHOSAssetProvider::GetType() const {
  return AssetResolver::AssetResolverType::kOHOSAssetProvider;
}

std::unique_ptr<fml::Mapping> OHOSAssetProvider::GetAsMapping(
    const std::string& asset_name) const {
  return impl_->GetAsMapping(asset_name);
}

std::unique_ptr<OHOSAssetProvider> OHOSAssetProvider::Clone() const {
  return std::make_unique<OHOSAssetProvider>(impl_);
}

bool OHOSAssetProvider::operator==(const AssetResolver& other) const {
  auto other_provider = other.as_ohos_asset_provider();
  if (!other_provider) {
    return false;
  }
  return impl_ == other_provider->impl_;
}

}  // namespace flutter
