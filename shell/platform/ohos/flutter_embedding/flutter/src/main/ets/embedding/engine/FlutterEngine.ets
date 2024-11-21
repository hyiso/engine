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

import common from '@ohos.app.ability.common';
import resourceManager from '@ohos.resourceManager';

import DartExecutor, { DartEntrypoint } from './dart/DartExecutor';
import FlutterNAPI from './FlutterNAPI';
import FlutterLoader from './loader/FlutterLoader';
import FlutterInjector from '../../FlutterInjector';
import FlutterEngineConnectionRegistry from './FlutterEngineConnectionRegistry';
import AbilityControlSurface from './plugins/ability/AbilityControlSurface';
import PluginRegistry from './plugins/PluginRegistry';
import LocalizationPlugin from '../../plugin/localization/LocalizationPlugin'
import PlatformViewsController from '../../plugin/platform/PlatformViewsController';
import { FlutterRenderer } from './renderer/FlutterRenderer';
import AccessibilityChannel from './systemchannels/AccessibilityChannel';
import LifecycleChannel from './systemchannels/LifecycleChannel';
import LocalizationChannel from './systemchannels/LocalizationChannel';
import MouseCursorChannel from './systemchannels/MouseCursorChannel';
import NavigationChannel from './systemchannels/NavigationChannel';
import PlatformChannel from './systemchannels/PlatformChannel';
import RestorationChannel from './systemchannels/RestorationChannel';
import SettingsChannel from './systemchannels/SettingsChannel';
import SystemChannel from './systemchannels/SystemChannel';
import TextInputChannel from './systemchannels/TextInputChannel';
import Log from '../../util/Log';

const TAG = "FlutterEngine";

/**
 * 操作FlutterEngin相关
 */
export default class FlutterEngine implements EngineLifecycleListener{

  private flutterNAPI: FlutterNAPI;
  private renderer: FlutterRenderer;
  private dartExecutor: DartExecutor;
  private pluginRegistry: FlutterEngineConnectionRegistry;
  private localizationPlugin: LocalizationPlugin;

  private accessibilityChannel: AccessibilityChannel;
  private lifecycleChannel: LifecycleChannel;
  private localizationChannel: LocalizationChannel;
  private mouseCursorChannel: MouseCursorChannel;
  private navigationChannel: NavigationChannel;
  private platformChannel: PlatformChannel;
  private restorationChannel: RestorationChannel;
  private settingsChannel: SettingsChannel;
  private systemChannel: SystemChannel;
  private textInputChannel: TextInputChannel;

  // Platform Views.
  private platformViewsController: PlatformViewsController;

  // Engine Lifecycle.
  private engineLifecycleListeners = new Set<EngineLifecycleListener>();

  /**
   * 需要初始化的工作：
   * 1、初始化DartExecutor
   * 2、初始化所有channel
   * 3、初始化plugin
   * 4、初始化flutterLoader
   * 5、初始化flutterNAPI
   * 6、engineLifecycleListeners
   */
  constructor(
      context: common.Context,
      flutterLoader: FlutterLoader | null,
      flutterNAPI: FlutterNAPI | null,
      platformViewsController: PlatformViewsController | null,
      dartVmArgs: Array<string> | null,
      waitForRestorationData: boolean) {
    const assetManager: resourceManager.ResourceManager = context.resourceManager;

    const injector: FlutterInjector = FlutterInjector.getInstance();

    if (flutterNAPI == null) {
      flutterNAPI = FlutterInjector.getInstance().getFlutterNAPI();
    }
    this.flutterNAPI = flutterNAPI;

    this.dartExecutor = new DartExecutor(this.flutterNAPI, assetManager);
    this.dartExecutor.onAttachedToNAPI();
    
    this.accessibilityChannel = new AccessibilityChannel(this.dartExecutor, this.flutterNAPI);
    this.lifecycleChannel = new LifecycleChannel(this.dartExecutor);
    this.localizationChannel = new LocalizationChannel(this.dartExecutor);
    this.mouseCursorChannel = new MouseCursorChannel(this.dartExecutor);
    this.navigationChannel = new NavigationChannel(this.dartExecutor);
    this.platformChannel = new PlatformChannel(this.dartExecutor);
    this.restorationChannel = new RestorationChannel(this.dartExecutor, waitForRestorationData);
    this.settingsChannel = new SettingsChannel(this.dartExecutor);
    this.systemChannel = new SystemChannel(this.dartExecutor);
    this.textInputChannel = new TextInputChannel(this.dartExecutor);

    this.localizationPlugin = new LocalizationPlugin(context, this.localizationChannel);

    if(flutterLoader == null){
      flutterLoader = injector.getFlutterLoader();
    }
  
    if (flutterNAPI.isAttached()) {
      flutterLoader.startInitialization(context)
      flutterLoader.ensureInitializationComplete(dartVmArgs);
    }
    this.flutterNAPI.addEngineLifecycleListener(this);
    this.flutterNAPI.setLocalizationPlugin(this.localizationPlugin);

    if(platformViewsController == null) {
      platformViewsController = new PlatformViewsController();
    }
  
    // It should typically be a fresh, unattached NAPI. But on a spawned engine, the NAPI instance
    // is already attached to a native shell. In that case, the Java FlutterEngine is created around
    // an existing shell.
    if (!this.flutterNAPI.isAttached()) {
      this.attachToNapi();
    }

    // TODO(mattcarroll): FlutterRenderer is temporally coupled to attach(). Remove that coupling if
    // possible.
    this.renderer = new FlutterRenderer(this.flutterNAPI);

    this.platformViewsController = platformViewsController;

    this.pluginRegistry =
        new FlutterEngineConnectionRegistry(
            context.getApplicationContext(), this, flutterLoader);

    this.localizationPlugin.sendLocalesToFlutter();
  }

  private attachToNapi(): void {
    Log.d(TAG, "Attaching to NAPI.");
    this.flutterNAPI.attachToNative();

    if (!this.isAttachedToNapi()) {
      throw new Error("FlutterEngine failed to attach to its native Object reference.");
    }
  }

  private isAttachedToNapi(): boolean {
    return this.flutterNAPI.isAttached();
  }

  spawn(context: common.Context,
        dartEntrypoint: DartEntrypoint,
        initialRoute: string,
        dartEntrypointArgs: Array<string>,
        platformViewsController: PlatformViewsController,
        waitForRestorationData: boolean): FlutterEngine {
    if (!this.isAttachedToNapi()) {
      throw new Error(
        "Spawn can only be called on a fully constructed FlutterEngine");
    }

    const newFlutterNAPI = this.flutterNAPI.spawn(
        dartEntrypoint.dartEntrypointFunctionName,
        dartEntrypoint.dartEntrypointLibrary,
        initialRoute,
        dartEntrypointArgs);
    return new FlutterEngine(
        context, // Context.
        null, // FlutterLoader. A null value passed here causes the constructor to get it from the
        // FlutterInjector.
        newFlutterNAPI, // FlutterJNI.
        platformViewsController, // PlatformViewsController.
        null, // String[]. The Dart VM has already started, this arguments will have no effect.
        waitForRestorationData); // boolean
  }

  destroy(): void {
    Log.d(TAG, "Destroying.");
    this.engineLifecycleListeners.forEach(listener => listener.onEngineWillDestroy())
    this.pluginRegistry.detachFromAbility();
    this.platformViewsController?.onDetachedFromNapi();
    this.pluginRegistry.destroy();
    this.dartExecutor.onDetachedFromNAPI();
    this.flutterNAPI.removeEngineLifecycleListener(this);
    this.flutterNAPI.detachFromNativeAndReleaseResources();
  }

  addEngineLifecycleListener(listener: EngineLifecycleListener): void {
    this.engineLifecycleListeners.add(listener);
  }

  removeEngineLifecycleListener(listener: EngineLifecycleListener): void {
    this.engineLifecycleListeners.delete(listener);
  }

  getDartExecutor(): DartExecutor {
    return this.dartExecutor
  }

  getRenderer(): FlutterRenderer {
    return this.renderer;
  }

  getAccessibilityChannel(): AccessibilityChannel {
    return this.accessibilityChannel;
  }

  getLifecycleChannel(): LifecycleChannel {
    return this.lifecycleChannel;
  }

  getLocalizationChannel(): LocalizationChannel {
    return this.localizationChannel;
  }

  getNavigationChannel(): NavigationChannel {
    return this.navigationChannel;
  }

  getPlatformChannel(): PlatformChannel {
    return this.platformChannel;
  }

  getRestorationChannel(): RestorationChannel {
    return this.restorationChannel;
  }

  getSettingsChannel(): SettingsChannel {
    return this.settingsChannel;
  }

  getSystemChannel(): SystemChannel {
    return this.systemChannel;
  }

  getMouseCursorChannel(): MouseCursorChannel {
    return this.mouseCursorChannel;
  }

  getTextInputChannel(): TextInputChannel {
    return this.textInputChannel;
  }

  getFlutterNAPI(): FlutterNAPI {
    return this.flutterNAPI;
  }

  getPlugins(): PluginRegistry {
    return this.pluginRegistry;
  }

  getLocalizationPlugin(): LocalizationPlugin {
    return this.localizationPlugin;
  }

  getPlatformViewsController(): PlatformViewsController {
    return this.platformViewsController;
  }

  getAbilityControlSurface(): AbilityControlSurface {
    return this.pluginRegistry;
  }

  onPreEngineRestart(): void {
    this.engineLifecycleListeners.forEach(listener => listener.onPreEngineRestart())
  }

  onEngineWillDestroy(): void {
    this.engineLifecycleListeners.forEach(listener => listener.onEngineWillDestroy())
  }
}

export interface EngineLifecycleListener {
  onPreEngineRestart(): void;

  onEngineWillDestroy(): void;
}