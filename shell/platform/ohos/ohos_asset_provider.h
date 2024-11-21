// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_OHOS_OHOS_ASSET_PROVIDER_H_
#define FLUTTER_SHELL_PLATFORM_OHOS_OHOS_ASSET_PROVIDER_H_

#include <rawfile/raw_file_manager.h>

#include "flutter/assets/asset_resolver.h"
#include "flutter/fml/memory/ref_counted.h"

namespace flutter {

class OHOSAssetProviderInternal {
 public:
  virtual std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const = 0;

 protected:
  virtual ~OHOSAssetProviderInternal() = default;
};

class OHOSAssetProvider final : public AssetResolver {
 public:
  explicit OHOSAssetProvider(NativeResourceManager* asssetManager,
                             std::string directory);

  explicit OHOSAssetProvider(std::shared_ptr<OHOSAssetProviderInternal> impl);

  ~OHOSAssetProvider() = default;

  // Returns a new 'std::unique_ptr<OHOSAssetProvider>' with the same 'impl_' as
  // this provider.
  std::unique_ptr<OHOSAssetProvider> Clone() const;

  // Obtain a raw pointer to the OHOSAssetProviderInternal.
  //
  // This method is intended for use in tests. Callers must not
  // delete the returned pointer.
  OHOSAssetProviderInternal* GetImpl() const { return impl_.get(); }

  bool operator==(const AssetResolver& other) const override;

 private:
  std::shared_ptr<OHOSAssetProviderInternal> impl_;

  // |flutter::AssetResolver|
  bool IsValid() const override;

  // |flutter::AssetResolver|
  bool IsValidAfterAssetManagerChange() const override;

  // |AssetResolver|
  AssetResolver::AssetResolverType GetType() const override;

  // |flutter::AssetResolver|
  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const override;

  // |AssetResolver|
  const OHOSAssetProvider* as_ohos_asset_provider() const override {
    return this;
  }

  FML_DISALLOW_COPY_AND_ASSIGN(OHOSAssetProvider);
};

}  // namespace flutter
#endif  // FLUTTER_SHELL_PLATFORM_OHOS_OHOS_ASSET_PROVIDER_H_
