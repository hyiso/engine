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

import FlutterNAPI from '../FlutterNAPI';
import FlutterApplicationInfo from './FlutterApplicationInfo';
import common from '@ohos.app.ability.common';
import StringUtils from '../../../util/StringUtils';
import ApplicationInfoLoader from './ApplicationInfoLoader';
import { bundleManager } from '@kit.AbilityKit';
import { display } from '@kit.ArkUI';
import fs from '@ohos.file.fs';

const TAG = "FlutterLoader";

const ENABLE_IMPELLER_META_DATA_KEY =
    "io.flutter.embedding.ohos.EnableImpeller";
const ENABLE_VULKAN_VALIDATION_META_DATA_KEY =
    "io.flutter.embedding.ohos.EnableVulkanValidation";
const IMPELLER_BACKEND_META_DATA_KEY =
    "io.flutter.embedding.ohos.ImpellerBackend";
const IMPELLER_OPENGL_GPU_TRACING_DATA_KEY =
    "io.flutter.embedding.ohos.EnableOpenGLGPUTracing";
const IMPELLER_VULKAN_GPU_TRACING_DATA_KEY =
    "io.flutter.embedding.ohos.EnableVulkanGPUTracing";

/**
 * Set whether leave or clean up the VM after the last shell shuts down.
 * Set it to true to leave the Dart VM,
 * set it to false to destroy VM.
 *
 * <p>If your want to let your app destroy the last shell and re-create shells more quickly, set
 * it to true, otherwise if you want to clean up the memory of the leak VM, set it to false.
 *
 * <p>TODO(eggfly): Should it be set to false by default?
 * https://github.com/flutter/flutter/issues/96843
 */
const LEAK_VM_META_DATA_KEY = "io.flutter.embedding.ohos.LeakVM";

// Must match values in flutter::switches
const AOT_SHARED_LIBRARY_NAME = "aot-shared-library-name";
const AOT_VMSERVICE_SHARED_LIBRARY_NAME = "aot-vmservice-shared-library-name";
const SNAPSHOT_ASSET_PATH_KEY = "snapshot-asset-path";
const VM_SNAPSHOT_DATA_KEY = "vm-snapshot-data";
const ISOLATE_SNAPSHOT_DATA_KEY = "isolate-snapshot-data";

// Resource names used for components of the precompiled snapshot.
const DEFAULT_LIBRARY = "libflutter.so";
const DEFAULT_KERNEL_BLOB = "kernel_blob.bin";
const VMSERVICE_SNAPSHOT_LIBRARY = "libvmservice_snapshot.so";

//文件路径分隔符
const FILE_SEPARATOR = "/";

const TIMESTAMP_PREFIX = "res_timestamp-";

function prefetchDefaultFontManager(): void {
  FlutterNAPI.prefetchDefaultFontManager();
}

/**
 * 定位在hap包中的flutter资源，并且加载flutter native library.
 */
export default class FlutterLoader {
  flutterNAPI: FlutterNAPI;
  initResult: InitResult | null = null;
  flutterApplicationInfo: FlutterApplicationInfo | null = null;
  context: common.Context | null = null;
  initialized: boolean = false;
  //初始化开始时间戳
  initStartTimestampMillis: number = 0;

  constructor(flutterNAPI: FlutterNAPI) {
    this.flutterNAPI = flutterNAPI;
  }

  /**
   * Starts initialization of the native system.
   *
   * <p>This loads the Flutter engine's native library to enable subsequent JNI calls. This also
   * starts locating and unpacking Dart resources packaged in the app's APK.
   *
   * <p>Calling this method multiple times has no effect.
   *
   * @param applicationContext The Android application context.
   * @param settings Configuration settings.
   */
  startInitialization(context: common.Context) {
    this.initStartTimestampMillis = Date.now();
    this.context = context;
    this.flutterApplicationInfo = ApplicationInfoLoader.load(context);
    prefetchDefaultFontManager();
    if (this.flutterApplicationInfo!.isDebugMode) {
      this.copyResource(context)
    }
    this.initResult = new InitResult(
      `${context.filesDir}/`,
      `${context.cacheDir}/`,
      `${context.filesDir}`
    )
  }


  private copyResource(context: common.Context) {
    let filePath = context.filesDir + FILE_SEPARATOR + this.flutterApplicationInfo!.flutterAssetsDir
    const timestamp = this.checkTimestamp(filePath);
    if (timestamp == null) {
      return;
    }
    if (this.context != null) {
      if (fs.accessSync(filePath + FILE_SEPARATOR + DEFAULT_KERNEL_BLOB)) {
        fs.rmdirSync(filePath);
      }

      if (!fs.accessSync(filePath)) {
        fs.mkdirSync(filePath)
      }

      let kernelBuffer = this.context.resourceManager.getRawFileContentSync(this.flutterApplicationInfo!.flutterAssetsDir + FILE_SEPARATOR + DEFAULT_KERNEL_BLOB)
      let kernelFile = fs.openSync(filePath + FILE_SEPARATOR + DEFAULT_KERNEL_BLOB, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE)
      fs.writeSync(kernelFile.fd, kernelBuffer.buffer)

      let vmBuffer = this.context.resourceManager.getRawFileContentSync(this.flutterApplicationInfo!.flutterAssetsDir + FILE_SEPARATOR + this.flutterApplicationInfo!.vmSnapshotData)
      let vmFile = fs.openSync(filePath + FILE_SEPARATOR + this.flutterApplicationInfo!.vmSnapshotData, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE)
      fs.writeSync(vmFile.fd, vmBuffer.buffer)

      let isolateBuffer = this.context.resourceManager.getRawFileContentSync(this.flutterApplicationInfo!.flutterAssetsDir + FILE_SEPARATOR + this.flutterApplicationInfo!.isolateSnapshotData)
      let isolateFile = fs.openSync(filePath + FILE_SEPARATOR + this.flutterApplicationInfo!.isolateSnapshotData, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE)
      fs.writeSync(isolateFile.fd, isolateBuffer.buffer)

      if (timestamp != null) {
        fs.openSync(filePath + FILE_SEPARATOR + timestamp, fs.OpenMode.READ_ONLY | fs.OpenMode.CREATE)
      }
    }
  }

  ensureInitializationComplete(args: Array<string> | null) {
    if (this.initialized) {
      return;
    }
    const shellArgs = new Array<string>();
    shellArgs.push("--icu-symbol-prefix=_binary_icudtl_dat");
    shellArgs.push(
      "--icu-native-lib-path="
        + this.flutterApplicationInfo!.nativeLibraryDir
        + FILE_SEPARATOR
        + DEFAULT_LIBRARY
    );
    if (args != null) {
      shellArgs.push(...args);
    }

    let kernelPath: string = "";
    if (this.flutterApplicationInfo!.isDebugMode) {
      const snapshotAssetPath = this.initResult!.dataDirPath + FILE_SEPARATOR + this.flutterApplicationInfo!.flutterAssetsDir;
      kernelPath = snapshotAssetPath + FILE_SEPARATOR + DEFAULT_KERNEL_BLOB;
      shellArgs.push("--" + SNAPSHOT_ASSET_PATH_KEY + "=" + snapshotAssetPath);
      shellArgs.push("--" + VM_SNAPSHOT_DATA_KEY + "=" + this.flutterApplicationInfo!.vmSnapshotData);
      shellArgs.push(
        "--" + ISOLATE_SNAPSHOT_DATA_KEY + "=" + this.flutterApplicationInfo!.isolateSnapshotData);
    } else {
      shellArgs.push(
        "--" + AOT_SHARED_LIBRARY_NAME + "=" + this.flutterApplicationInfo!.aotSharedLibraryName);
      // Most devices can load the AOT shared library based on the library name
      // with no directory path.  Provide a fully qualified path to the library
      // as a workaround for devices where that fails.
      shellArgs.push(
        "--"
          + AOT_SHARED_LIBRARY_NAME
          + "="
          + this.flutterApplicationInfo!.nativeLibraryDir
          + FILE_SEPARATOR
          + this.flutterApplicationInfo!.aotSharedLibraryName);

      // In profile mode, provide a separate library containing a snapshot for
      // launching the Dart VM service isolate.
      if (this.flutterApplicationInfo!.isProfile) {
        shellArgs.push("--" + AOT_VMSERVICE_SHARED_LIBRARY_NAME + "=" + VMSERVICE_SNAPSHOT_LIBRARY);
      }
    }

    shellArgs.push("--cache-dir-path=" + this.initResult!.engineCachesPath);
    if (StringUtils.isNotEmpty(this.flutterApplicationInfo!.domainNetworkPolicy)) {
      shellArgs.push("--domain-network-policy=" + this.flutterApplicationInfo!.domainNetworkPolicy);
    }


    const flag = bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_HAP_MODULE
      | bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_METADATA;
    const bundleInfo = bundleManager.getBundleInfoForSelfSync(flag);
    const metadata = bundleInfo.hapModulesInfo[0]?.metadata?.reduce((acc, cur) => {
      acc[cur.name] = cur.value;
      return acc;
    }, {} as Record<string, string>);

    const defaultDisplay = display.getDefaultDisplaySync();
    const screenWidth = defaultDisplay.width;
    const screenHeight = defaultDisplay.height;
    const resourceCacheMaxBytesThreshold = screenWidth * screenHeight * 12 * 4;
    shellArgs.push("--resource-cache-max-bytes-threshold=" + resourceCacheMaxBytesThreshold);

    shellArgs.push("--prefetched-default-font-manager");

    if (metadata != null) {
      if (metadata[ENABLE_IMPELLER_META_DATA_KEY] == 'true') {
        shellArgs.push("--enable-impeller");
      }
      if (metadata[ENABLE_VULKAN_VALIDATION_META_DATA_KEY] == 'true') {
        shellArgs.push("--enable-vulkan-validation");
      }
      if (metadata[IMPELLER_OPENGL_GPU_TRACING_DATA_KEY] == 'true') {
        shellArgs.push("--enable-opengl-gpu-tracing");
      }
      if (metadata[IMPELLER_VULKAN_GPU_TRACING_DATA_KEY] == 'true') {
        shellArgs.push("--enable-vulkan-gpu-tracing");
      }
      const backend = metadata[IMPELLER_BACKEND_META_DATA_KEY];
      if (backend != null) {
        shellArgs.push("--impeller-backend=" + backend);
      }
    }

    const leakVM = FlutterLoader.isLeakVM(metadata) ? 'true' : 'false';
    shellArgs.push("--leak-vm=" + leakVM);

    const initTimeMillis = Date.now() - this.initStartTimestampMillis;
    this.flutterNAPI.init(
      this.context!,
      shellArgs,
      kernelPath,
      this.initResult!.appStoragePath,
      this.initResult!.engineCachesPath!,
      initTimeMillis
    );
    this.initialized = true;
  }

  static isLeakVM(metadata: Record<string, string>|null): boolean {
    let leakVMDefaultValue = false;
    if (!metadata) {
      return leakVMDefaultValue;
    }
    return metadata[LEAK_VM_META_DATA_KEY] == 'true';
  }

  findAppBundlePath(): string {
    return this.flutterApplicationInfo == null ? "" : this.flutterApplicationInfo!.flutterAssetsDir;
  }

  getLookupKeyForAsset(asset: string, packageName?: string): string {
    return this.fullAssetPathFrom(asset);
  }

  fullAssetPathFrom(filePath: string): string {
    return this.flutterApplicationInfo == null ? "" : this.flutterApplicationInfo!.flutterAssetsDir + "/" + filePath;
  }

  private checkTimestamp(dataDir: string) : string | null {
    let bundleInfo = bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT);
    const expectedTimestamp = TIMESTAMP_PREFIX + bundleInfo.versionCode + "-" + bundleInfo.updateTime;
    const existingTimestamps = this.getExistingTimestamps(dataDir);
    if (existingTimestamps == null) {
      return expectedTimestamp;
    }

    if (existingTimestamps.length != 1 || !(expectedTimestamp == existingTimestamps[0])) {
      return expectedTimestamp;
    }

    return null;
  }

  private getExistingTimestamps(dataDir: string): string[] {
    return fs.accessSync(dataDir) ? fs.listFileSync(dataDir, {
      filter: {
        displayName: [`${TIMESTAMP_PREFIX}*`]
      }
    }) : new Array();
  }

  isInitialized(): boolean {
    return this.initialized;
  }

}

class InitResult {
  appStoragePath: string;
  engineCachesPath: string;
  dataDirPath: string;

  constructor(appStoragePath: string,
              engineCachesPath: string,
              dataDirPath: string) {
    this.appStoragePath = appStoragePath;
    this.engineCachesPath = engineCachesPath;
    this.dataDirPath = dataDirPath;
  }
}
