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

import flutter from 'libflutter.so';
import common from '@ohos.app.ability.common';
import Log from '../../util/Log';
import resourceManager from '@ohos.resourceManager';
import { PlatformMessageHandler } from './dart/PlatformMessageHandler';
import { FlutterCallbackInformation } from '../../view/FlutterCallbackInformation';
import { EngineLifecycleListener } from './FlutterEngine';
import { ByteBuffer } from '../../util/ByteBuffer';
import { Action } from '../../view/AccessibilityBridge'
import LocalizationPlugin from '../../plugin/localization/LocalizationPlugin';
import intl from '@ohos.intl';
import FlutterManager from '../ohos/FlutterManager';

const TAG = "FlutterNAPI";

export default class FlutterNAPI {

  nativeShellHolderId: number | null = null;
  platformMessageHandler: PlatformMessageHandler | null = null;
  private engineLifecycleListeners = new Set<EngineLifecycleListener>();
  accessibilityDelegate: AccessibilityDelegate | null = null;
  localizationPlugin: LocalizationPlugin | null = null;

  isDisplayingFlutterUi: boolean = false;

  updateRefreshRate(refreshRateFPS : number) {
    flutter.nativeUpdateRefreshRate(refreshRateFPS);
  }

  init(context: common.Context,
       args: Array<string>,
       bundlePath: string,
       appStoragePath: string,
       engineCachesPath: string,
       initTimeMillis: number) {
    if (FlutterNAPI.initCalled) {
      Log.w(TAG, "FlutterNAPI.init called more than once");
      return;
    }
    flutter.nativeInit(context, args, bundlePath, appStoragePath,
      engineCachesPath, initTimeMillis);
    FlutterNAPI.initCalled = true;
  }
  private static initCalled: boolean = false;

  // This is set from native code via NAPI.
  private static vmServiceUri:string = "";

  static getVMServiceUri(): string {
    return FlutterNAPI.vmServiceUri;
  }

  static prefetchDefaultFontManager(): void {
    flutter.nativePrefetchDefaultFontManager();
  }

  attachToNative(): void {
    if (this.isAttached()) {
      Log.w(TAG, "FlutterNAPI.attachToNative called more than once");
      return;
    }
    this.nativeShellHolderId = flutter.nativeAttach(this);
  }

  runBundleAndSnapshotFromLibrary(
    bundlePath: string,
    entrypointFunctionName: string | undefined,
    pathToEntrypointFunction: string | undefined,
    assetManager: resourceManager.ResourceManager,
    entrypointArgs: Array<string>) {
    if (!this.isAttached()) {
      Log.e(TAG, "runBundleAndSnapshotFromLibrary this.nativeShellHolderId:" + this.nativeShellHolderId)
      return;
    }
    flutter.nativeRunBundleAndSnapshotFromLibrary(this.nativeShellHolderId!, bundlePath, entrypointFunctionName, pathToEntrypointFunction, assetManager, entrypointArgs);
  };

  setPlatformMessageHandler(platformMessageHandler: PlatformMessageHandler | null): void {
    this.ensureRunningOnMainThread();
    this.platformMessageHandler = platformMessageHandler;
  }

  static nativeLookupCallbackInformation(handle: number): FlutterCallbackInformation {
    return flutter.nativeLookupCallbackInformation(handle);
  }

  notifyLowMemoryWarning(): void {
    this.ensureRunningOnMainThread();
    if (!this.isAttached()) {
      Log.e(TAG, "notifyLowMemoryWarning this.nativeShellHolderId:" + this.nativeShellHolderId)
      return;
    }
    flutter.nativeNotifyLowMemoryWarning(this.nativeShellHolderId!);
  }

  isAttached(): boolean {
    return this.nativeShellHolderId != null;
  }

  private ensureRunningOnMainThread(): void {

  }

  dispatchEmptyPlatformMessage(channel: String, responseId: number): void {
    this.ensureRunningOnMainThread();
    if (this.isAttached()) {
      flutter.nativeDispatchEmptyPlatformMessage(this.nativeShellHolderId!, channel, responseId);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message to Flutter, but FlutterNAPI was detached from native C++. Could not send. Channel: "
          + channel
          + ". Response ID: "
          + responseId);
    }
  }

  /** Sends a reply {@code message} from Android to Flutter over the given {@code channel}. */
  dispatchPlatformMessage(channel: String, message: ArrayBuffer, position: number, responseId: number): void {
    this.ensureRunningOnMainThread();
    if (this.isAttached()) {
      flutter.nativeDispatchPlatformMessage(this.nativeShellHolderId!, channel, message, position, responseId);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message to Flutter, but FlutterNAPI was detached from native C++. Could not send. Channel: "
          + channel
          + ". Response ID: "
          + responseId);
    }
  }

  invokePlatformMessageEmptyResponseCallback(responseId: number): void {
    if (this.isAttached()) {
      flutter.nativeInvokePlatformMessageEmptyResponseCallback(this.nativeShellHolderId!, responseId);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message response, but FlutterNAPI was detached from native C++. Could not send. Response ID: "
          + responseId);
    }
  }

  invokePlatformMessageResponseCallback(responseId: number, message: ArrayBuffer, position: number) {
    if (this.isAttached()) {
      flutter.nativeInvokePlatformMessageResponseCallback(
        this.nativeShellHolderId!, responseId, message, position);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message response, but FlutterNAPI was detached from native C++. Could not send. Response ID: "
          + responseId);
    }
  }

  setViewportMetrics(devicePixelRatio: number, physicalWidth: number
                     , physicalHeight: number, physicalPaddingTop: number, physicalPaddingRight: number
                     , physicalPaddingBottom: number, physicalPaddingLeft: number, physicalViewInsetTop: number
                     , physicalViewInsetRight: number, physicalViewInsetBottom: number, physicalViewInsetLeft: number
                     , systemGestureInsetTop: number, systemGestureInsetRight: number, systemGestureInsetBottom: number
                     , systemGestureInsetLeft: number, physicalTouchSlop: number, displayFeaturesBounds: Array<number>
                     , displayFeaturesType: Array<number>, displayFeaturesState: Array<number>): void {
    if (this.isAttached()) {
      flutter.nativeSetViewportMetrics(this.nativeShellHolderId!, devicePixelRatio,
        physicalWidth,
        physicalHeight,
        physicalPaddingTop,
        physicalPaddingRight,
        physicalPaddingBottom,
        physicalPaddingLeft,
        physicalViewInsetTop,
        physicalViewInsetRight,
        physicalViewInsetBottom,
        physicalViewInsetLeft,
        systemGestureInsetTop,
        systemGestureInsetRight,
        systemGestureInsetBottom,
        systemGestureInsetLeft,
        physicalTouchSlop,
        displayFeaturesBounds,
        displayFeaturesType,
        displayFeaturesState);
    }
  }

  spawn(entrypointFunctionName: string, pathToEntrypointFunction: string, initialRoute: string, entrypointArgs: Array<string>): FlutterNAPI {
    return flutter.nativeSpawn(
      this.nativeShellHolderId,
      entrypointFunctionName,
      pathToEntrypointFunction,
      initialRoute,
      entrypointArgs,
      FlutterNAPI,
    );
  }

  dispatchPointerDataPacket(data: ArrayBuffer, position: number): void {
    if (this.isAttached()) {
      flutter.nativeDispatchPointerDataPacket(this.nativeShellHolderId!, data, position);
    }
  }

  addEngineLifecycleListener(engineLifecycleListener: EngineLifecycleListener): void {
    this.engineLifecycleListeners.add(engineLifecycleListener);
  }

  removeEngineLifecycleListener(engineLifecycleListener: EngineLifecycleListener) {
    this.engineLifecycleListeners.delete(engineLifecycleListener);
  }

  /**
   * Destroys the resources provided sent to `handlePlatformMessage`.
   *
   * <p>This can be called on any thread.
   *
   * @param messageData the argument sent to handlePlatformMessage.
   */
  cleanupMessageData(messageData: number): void {
    // This doesn't rely on being attached like other methods.
    flutter.nativeCleanupMessageData(messageData);
  }

  // Called by native.
  handlePlatformMessage(channel: string, message: ArrayBuffer, replyId: number, messageData: number): void {
    Log.d(TAG, "called handlePlatformMessage Channel: " + channel + ". Response ID: " + replyId);
    if (this.platformMessageHandler != null) {
      this.platformMessageHandler.handleMessageFromDart(channel, message, replyId, messageData);
    } else {
      flutter.nativeCleanupMessageData(messageData);
    }
  }

  //Called by native to respond to a platform message that we sent.
  handlePlatformMessageResponse(replyId: number, reply: ArrayBuffer): void {
    Log.d(TAG, "called handlePlatformMessageResponse Response ID: " + replyId);
    if (this.platformMessageHandler != null) {
      this.platformMessageHandler.handlePlatformMessageResponse(replyId, reply);
    }
  }

  // Called by native to notify first Flutter frame rendered.
  onFirstFrame(): void {
    this.isDisplayingFlutterUi = true;
    Log.d(TAG, "called onFirstFrame")
    FlutterManager.getInstance().getFlutterViewList().forEach((value)=> {
      value.onFirstFrame();
    });
  }

  // Called by native.
  onPreEngineRestart(): void {
    Log.d(TAG, "called onPreEngineRestart")
    this.engineLifecycleListeners.forEach( listener =>  listener.onPreEngineRestart());
  }

  setSemanticsEnabled(enabled: boolean, responseId: number): void {
    if (this.isAttached()) {
      flutter.nativeSetSemanticsEnabled(this.nativeShellHolderId, enabled);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message response, but FlutterNAPI was detached from native C++. Could not send. Response ID: "
          + responseId);
    }
  }

  setAccessibilityFeatures(flags: number): void {
    if (this.isAttached()) {
      flutter.nativeSetAccessibilityFeatures(this.nativeShellHolderId, flags);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message response, but FlutterNAPI was detached from native C++. Could not send.");
    }
  }

  dispatchSemanticsAction(virtualViewId: number, action: Action, responseId: number): void {
    if (this.isAttached()) {
      this.nativeDispatchSemanticsAction(virtualViewId, action, responseId);
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message response, but FlutterNAPI was detached from native C++. Could not send. Response ID: "
          + responseId);
    }
  }

  nativeDispatchSemanticsAction(virtualViewId: number, action: Action, responseId: number): void {}

  setAccessibilityDelegate(delegate: AccessibilityDelegate, responseId: number): void {
    if (this.isAttached()) {
      this.accessibilityDelegate = delegate;
    } else {
      Log.w(
        TAG,
        "Tried to send a platform message response, but FlutterNAPI was detached from native C++. Could not send. Response ID: "
          + responseId);
    }
  }

  setLocalizationPlugin(localizationPlugin: LocalizationPlugin | null): void {
    this.localizationPlugin = localizationPlugin;
  }

  computePlatformResolvedLocale(strings: Array<string>): Array<string> {
    if (!this.localizationPlugin) {
      return [];
    }
    let supportedLocales: intl.Locale[] = [];
    const localeDataLength = 3;
    for (let i = 0; i < strings.length; i += localeDataLength) {
      let localeParts: string[] = [];
      if (strings[i].length > 0) {
        localeParts.push(strings[i]);
      }
      if (strings[i + 1].length > 0) {
        localeParts.push(strings[i + 1]);
      }
      if (strings[i + 2].length > 0) {
        localeParts.push(strings[i + 2]);
      }
      supportedLocales.push(new intl.Locale(localeParts.join('-')));
    }

    const result = this.localizationPlugin.resolveNativeLocale(supportedLocales);
    if (!result) {
      return [];
    }
    return [result.language, result.region, result.script];
  }

  getScaledFontSize(fontSize: number, configurationId: number): number {
    return vp2px(fontSize);
  }

  /**
   * xcomponet绑定flutterEngine
   * @param xcomponentId
   */
  xComponentAttachFlutterEngine(xcomponentId: string) {
    flutter.nativeXComponentAttachFlutterEngine(this.nativeShellHolderId!, xcomponentId);
  }

  /**
   * xcomponet解除绑定flutterEngine
   * @param xcomponentId
   */
  xComponentDetachFlutterEngine(xcomponentId: string) {
    flutter.nativeXComponentDetachFlutterEngine(this.nativeShellHolderId!, xcomponentId);
  }

  onSurfaceCreated(surfaceId: string) {
    flutter.nativeSurfaceCreated(this.nativeShellHolderId!, BigInt(surfaceId));
  }

  onSurfaceChanged(width: number, height: number) {
    flutter.nativeSurfaceChanged(this.nativeShellHolderId!, width, height);
  }

  onSurfaceDestroyed() {
    flutter.nativeSurfaceDestroyed(this.nativeShellHolderId!);
  }

  /**
   * xcomponent send mouseWheel event to flutterEngine
   * @param xcomponentId
   * @param eventType
   * @param event
   */
  xComponentDisPatchMouseWheel(xcomponentId: string, eventType: string, event: PanGestureEvent) {
    // only mouse
    if (event.source !== SourceType.Mouse) {
      return;
    }
    const vaildFinger = event.fingerList?.find(item => item.globalX && item.globalY);
    if (!vaildFinger) {
      return;
    }
    flutter.nativeXComponentDispatchMouseWheel(
      this.nativeShellHolderId!!,
      xcomponentId,
      eventType,
      vaildFinger?.id,
      vaildFinger?.localX,
      vaildFinger?.localY,
      event.offsetY,
      event.timestamp
    );
  }

  detachFromNativeAndReleaseResources() {
    if (!this.isAttached()) {
      Log.e(TAG, "detachFromNativeAndReleaseResources this.nativeShellHolderId:" + this.nativeShellHolderId)
      return;
    }
    flutter.nativeDestroy(this.nativeShellHolderId!);
    this.nativeShellHolderId = null;
  }

  unregisterTexture(textureId: number): void {
    Log.d(TAG, "called unregisterTexture ");
    if (!this.isAttached()) {
      Log.e(TAG, "unregisterTexture this.nativeShellHolderId:" + this.nativeShellHolderId)
      return;
    }
    flutter.nativeUnregisterTexture(this.nativeShellHolderId!, textureId);
  }

  registerTexture(textureId: number): number {
    Log.d(TAG, "called registerTexture ");
    if (!this.isAttached()) {
      Log.e(TAG, "registerTexture this.nativeShellHolderId:" + this.nativeShellHolderId)
      return 0;
    }
    return flutter.nativeRegisterTexture(this.nativeShellHolderId!, textureId);
  }

  registerSurfaceTexture(textureId: number, surfaceTexture: flutter.SurfaceTexture): void {
    Log.d(TAG, "called registerTexture ");
    if (!this.isAttached()) {
      Log.e(TAG, "registerTexture this.nativeShellHolderId:" + this.nativeShellHolderId)
      return;
    }
    flutter.nativeRegisterSurfaceTexture(this.nativeShellHolderId!, textureId, surfaceTexture);
  }

  setTextureBufferSize(textureId: number, width: number, height: number): void {
    Log.d(TAG, "called setTextureBufferSize ");
    if (!this.isAttached()) {
      Log.e(TAG, "setTextureBufferSize this.nativeShellHolderId:" + this.nativeShellHolderId)
      return;
    }
    flutter.nativeSetTextureBufferSize(this.nativeShellHolderId!, textureId, width, height);
  }

}

export interface AccessibilityDelegate {
  updateCustomAccessibilityActions(buffer: ByteBuffer, strings: string[]): void;

  updateSemantics(buffer: ByteBuffer, strings: string[], stringAttributeArgs: ByteBuffer[]): void;
}
