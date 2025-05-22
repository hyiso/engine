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
import display from '@ohos.display';
import window from '@ohos.window';
import ConfigurationConstant from '@ohos.app.ability.ConfigurationConstant';
import { Configuration } from '@ohos.app.ability.Configuration';
import { JSON } from '@kit.ArkTS';

import { AbilityConstant } from '@kit.AbilityKit';
import I18n from '@ohos.i18n';

import Log from '../util/Log';
import KeyboardManager from '../embedding/ohos/KeyboardManager';
import MouseCursorPlugin from '../plugin/mouse/MouseCursorPlugin';
import FlutterManager from '../embedding/ohos/FlutterManager';
import { DVModel, DVModelChildren, DVModelEvents, DVModelParameters } from './DynamicView/dynamicView';
import { PlatformBrightness } from '../embedding/engine/systemchannels/SettingsChannel'
import FlutterEngine from '../embedding/engine/FlutterEngine';
import { EmbeddingNodeController } from '../embedding/ohos/EmbeddingNodeController';
import PlatformView, { Params } from '../plugin/platform/PlatformView';
import OhosTouchProcessor from '../embedding/ohos/OhosTouchProcessor.ets';
import { FlutterRenderer } from '../embedding/engine/renderer/FlutterRenderer.ets';
import { FlutterUiDisplayListener } from '../embedding/engine/renderer/FlutterUiDisplayListener';

const TAG = "FlutterView";

export class ViewportMetrics {
  devicePixelRatio: number = 1.0;
  physicalWidth: number = 0;
  physicalHeight: number = 0;
  physicalViewPaddingTop: number = 0;
  physicalViewPaddingRight: number = 0;
  physicalViewPaddingBottom: number = 0;
  physicalViewPaddingLeft: number = 0;
  physicalViewInsetTop: number = 0;
  physicalViewInsetRight: number = 0;
  physicalViewInsetBottom: number = 0;
  physicalViewInsetLeft: number = 0;
  systemGestureInsetTop: number = 0;
  systemGestureInsetRight: number = 0;
  systemGestureInsetBottom: number = 0;
  systemGestureInsetLeft: number = 0;
  physicalTouchSlop: number = -1;

  clone(): ViewportMetrics {
    const copy = new ViewportMetrics();
    copy.devicePixelRatio = this.devicePixelRatio;
    copy.physicalWidth = this.physicalWidth;
    copy.physicalHeight = this.physicalHeight;
    copy.physicalViewPaddingTop = this.physicalViewPaddingTop;
    copy.physicalViewPaddingRight = this.physicalViewPaddingRight;
    copy.physicalViewPaddingBottom = this.physicalViewPaddingBottom;
    copy.physicalViewPaddingLeft = this.physicalViewPaddingLeft;
    copy.physicalViewInsetTop = this.physicalViewInsetTop;
    copy.physicalViewInsetRight = this.physicalViewInsetRight;
    copy.physicalViewInsetBottom = this.physicalViewInsetBottom;
    copy.physicalViewInsetLeft = this.physicalViewInsetLeft;
    copy.systemGestureInsetTop = this.systemGestureInsetTop;
    copy.systemGestureInsetRight = this.systemGestureInsetRight;
    copy.systemGestureInsetBottom = this.systemGestureInsetBottom;
    copy.systemGestureInsetLeft = this.systemGestureInsetLeft;
    copy.physicalTouchSlop = this.physicalTouchSlop;
    return copy;
  }

  isEqual(other: ViewportMetrics): boolean {
    return this.devicePixelRatio === other.devicePixelRatio &&
      this.physicalWidth === other.physicalWidth &&
      this.physicalHeight === other.physicalHeight &&
      this.physicalViewPaddingTop === other.physicalViewPaddingTop &&
      this.physicalViewPaddingRight === other.physicalViewPaddingRight &&
      this.physicalViewPaddingBottom === other.physicalViewPaddingBottom &&
      this.physicalViewPaddingLeft === other.physicalViewPaddingLeft &&
      this.physicalViewInsetTop === other.physicalViewInsetTop &&
      this.physicalViewInsetRight === other.physicalViewInsetRight &&
      this.physicalViewInsetBottom === other.physicalViewInsetBottom &&
      this.physicalViewInsetLeft === other.physicalViewInsetLeft &&
      this.systemGestureInsetTop === other.systemGestureInsetTop &&
      this.systemGestureInsetRight === other.systemGestureInsetRight &&
      this.systemGestureInsetBottom === other.systemGestureInsetBottom &&
      this.systemGestureInsetLeft === other.systemGestureInsetLeft &&
      this.physicalTouchSlop === other.physicalTouchSlop;
  }
}

export class PlatformViewParas {
  width: number = 0.0;
  height: number = 0.0;
  top: number = 0.0;
  left: number = 0.0;
  direction: Direction = Direction.Auto;

  setValue(width: number, height: number, top: number, left: number): void {
    this.width = width;
    this.height = height;
    this.top = top;
    this.left = left;
  }

  setOffset(top: number, left: number): void {
    this.top = top;
    this.left = left;
  }
}

export class FlutterView extends XComponentController {

  private context: Context;
  private flutterEngine: FlutterEngine | null = null
  private id: string = ""
  private dVModel: DVModel = new DVModel("Stack", new DVModelParameters(), new DVModelEvents(), new DVModelChildren(), null);
  private wrapBuilder: WrappedBuilder<[Params]> | undefined = undefined;
  private platformView: PlatformView | undefined = undefined;
  // private isSurfaceAvailableForRendering: boolean = false
  private viewportMetrics = new ViewportMetrics();
  private displayInfo?: display.Display;
  private keyboardManager: KeyboardManager | null = null;
  private ohosTouchProcessor: OhosTouchProcessor | null = null;
  private mainWindow: window.Window | null = null;
  private mouseCursorPlugin?: MouseCursorPlugin;
  private flutterUiDisplayListeners: Set<FlutterUiDisplayListener> = new Set();
  private flutterUiDisplayListener: FlutterUiDisplayListener = Object({
    onFlutterUiDisplayed: () => {
      this.isFlutterUiDisplayed = true;
      this.flutterUiDisplayListeners.forEach((listener) => {
        listener.onFlutterUiDisplayed();
      });
    },
    onFlutterUiNoLongerDisplayed: () => {
      this.isFlutterUiDisplayed = false;
      this.flutterUiDisplayListeners.forEach((listener) => {
        listener.onFlutterUiNoLongerDisplayed();
      });
    }
  });
  private isFlutterUiDisplayed: boolean = false;
  private surfaceId: string = "0";
  private nodeController: EmbeddingNodeController = new EmbeddingNodeController();
  private platformViewSize: PlatformViewParas = new PlatformViewParas();
  private checkFullScreen: boolean = true;
  private checkKeyboard: boolean = true;
  private checkGesture: boolean = true;
  private systemAvoidArea: window.AvoidArea;
  private navigationAvoidArea: window.AvoidArea;
  private gestureAvoidArea: window.AvoidArea;
  private keyboardAvoidArea: window.AvoidArea;

  private environmentCallbackId: number = -1;


  constructor(viewId: string, context: Context) {
    super();
    this.context = context;
    this.id = viewId
    this.displayInfo = display.getDefaultDisplaySync();
    this.viewportMetrics.devicePixelRatio = this.displayInfo?.densityPixels;
    this.viewportMetrics.physicalTouchSlop = 1.0 * this.displayInfo?.densityPixels;

    const mainWindow = FlutterManager.getInstance()
      .getWindowStage(FlutterManager.getInstance().getUIAbility(context))
      .getMainWindowSync();

    mainWindow.on('windowSizeChange', this.windowSizeChangeCallback);
    mainWindow.on('avoidAreaChange', this.avoidAreaChangeCallback);
    mainWindow.on('windowStatusChange', this.windowStatusChangeCallback);
    this.systemAvoidArea = mainWindow.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM);
    this.navigationAvoidArea = mainWindow.getWindowAvoidArea(window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR);
    this.gestureAvoidArea = mainWindow.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM_GESTURE);
    this.keyboardAvoidArea = mainWindow.getWindowAvoidArea(window.AvoidAreaType.TYPE_KEYBOARD);
  }

  private windowSizeChangeCallback = (data: window.Size) => {
    if (this.isAttachedToFlutterEngine()) {
      this.onAreaChange(null);
    }
  }

  private avoidAreaChangeCallback = (data: window.AvoidAreaOptions) => {
    if (this.isAttachedToFlutterEngine()) {
      if (data.type == window.AvoidAreaType.TYPE_SYSTEM) { //0
        this.systemAvoidArea = data.area;
      } else if (data.type == window.AvoidAreaType.TYPE_SYSTEM_GESTURE) { //2
        this.gestureAvoidArea = data.area;
      } else if (data.type == window.AvoidAreaType.TYPE_KEYBOARD) { //3
        this.keyboardAvoidArea = data.area;
      } else if (data.type == window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR) { //4
        this.navigationAvoidArea = data.area;
      }
      this.onAreaChange(null);
    }
  }

  private windowStatusChangeCallback = (data: window.WindowStatusType) => {
    if (this.isAttachedToFlutterEngine()) {
      FlutterManager.getInstance().getFullScreenListener().onScreenStateChanged(data);
    }
  };

  getId(): string {
    return this.id
  }

  setSurfaceId(surfaceId: string): void {
    this.surfaceId = surfaceId;
  }

  getSurfaceId(): string {
    return this.surfaceId;
  }

  onSurfaceCreated(surfaceId: string): void {
    this.flutterEngine?.getFlutterNAPI().onSurfaceCreated(surfaceId);
  }

  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    this.flutterEngine?.getFlutterNAPI().onSurfaceChanged(rect.surfaceWidth, rect.surfaceHeight);
  }

  onSurfaceDestroyed(surfaceId: string): void {
    this.flutterEngine?.getFlutterNAPI().onSurfaceDestroyed();
  }

  getEmbeddingNodeController(): EmbeddingNodeController {
    return this.nodeController;
  }

  setWrappedBuilder(wrappedBuilder: WrappedBuilder<[Params]>) {
    this.wrapBuilder = wrappedBuilder;
  }

  getWrappedBuilder(): WrappedBuilder<[Params]> | undefined {
    return this.wrapBuilder;
  }

  setPlatformView(platformView: PlatformView) {
    this.platformView = platformView;
  }

  getPlatformView(): PlatformView | undefined {
    return this.platformView;
  }

  getPlatformViewSize(): PlatformViewParas {
    return this.platformViewSize;
  }

  getDVModel() {
    return this.dVModel
  }

  onDestroy() {
    const mainWindow = FlutterManager.getInstance()
      .getWindowStage(FlutterManager.getInstance().getUIAbility(this.context))
      .getMainWindowSync();
    try {
      mainWindow.off('windowSizeChange', this.windowSizeChangeCallback);
      mainWindow.off('avoidAreaChange', this.avoidAreaChangeCallback);
      mainWindow.off('windowStatusChange', this.windowStatusChangeCallback);
    } catch (e) {
      Log.e(TAG, "mainWindow off error: " + JSON.stringify(e));
    }
  }

  attachToFlutterEngine(flutterEngine: FlutterEngine): void {
    Log.d(TAG, "Attaching to a FlutterEngine: " + flutterEngine);
    if (this.isAttachedToFlutterEngine()) {
      if (flutterEngine == this.flutterEngine) {
        Log.d(TAG, "Already attached to this engine. Doing nothing.");
        return;
      }
      // Detach from a previous FlutterEngine so we can attach to this new one.f
      Log.d(
        TAG,
        "Currently attached to a different engine. Detaching and then attaching"
          + " to new engine.");
      this.detachFromFlutterEngine();
    }
    this.flutterEngine = flutterEngine;

    // Instruct our FlutterRenderer that we are now its designated RenderSurface.
    const flutterRenderer: FlutterRenderer = this.flutterEngine.getRenderer();
    this.isFlutterUiDisplayed = flutterRenderer.isDisplayingFlutterUi();
    flutterRenderer.addIsDisplayingFlutterUiListener(this.flutterUiDisplayListener);

    this.keyboardManager = new KeyboardManager(flutterEngine);
    this.ohosTouchProcessor = new OhosTouchProcessor(flutterEngine.getRenderer(), false);
    this.flutterEngine?.getFlutterNAPI()?.updateRefreshRate(this.displayInfo!.refreshRate);
    flutterEngine.getPlatformViewsController()?.attachToView(this);
    this.updateViewportMetrics()

    let newArea: Area | null = {
      width: px2vp(this.displayInfo!.width),
      height: px2vp(this.displayInfo!.height),
      position: { x: 0, y: 0 },
      globalPosition: { x: 0, y: 0 }
    };
    if (this.viewportMetrics.physicalWidth != 0 || this.viewportMetrics.physicalHeight != 0) {
      newArea = null;
    }
    this.onAreaChange(newArea, true);

    const mainWindow = FlutterManager.getInstance()
      .getWindowStage(FlutterManager.getInstance().getUIAbility(this.context))
      .getMainWindowSync();
    let windowId = mainWindow.getWindowProperties().id;
    this.mouseCursorPlugin = new MouseCursorPlugin(windowId, this.flutterEngine?.getMouseCursorChannel()!);
    this.sendUserSettingsToFlutter();
    this.environmentCallbackId = this.context.getApplicationContext().on('environment', {
      onConfigurationUpdated: (config: Configuration) => {
        Log.d(TAG, "onConfigurationUpdated: " + JSON.stringify(config));
        this.flutterEngine?.getSettingsChannel()?.startMessage()
          .setNativeSpellCheckServiceDefined(false)
          .setBrieflyShowPassword(false)
          .setAlwaysUse24HourFormat(I18n.System.is24HourClock())
          .setPlatformBrightness(config.colorMode != ConfigurationConstant.ColorMode.COLOR_MODE_DARK
            ? PlatformBrightness.LIGHT : PlatformBrightness.DARK)
          .setTextScaleFactor(config.fontSizeScale == undefined? 1.0 : config.fontSizeScale)
          .send(); //热启动生命周期内，实时监听系统设置环境改变并实时发送相应信息
      },

      onMemoryLevel: (level: AbilityConstant.MemoryLevel) => {
        Log.d(TAG, "onMemoryLevel: " + JSON.stringify(level));
      }
    });
    if (this.isFlutterUiDisplayed) {
      this.flutterUiDisplayListener.onFlutterUiDisplayed();
    }
  }

  detachFromFlutterEngine(): void {
    Log.d(TAG, "Detaching from a FlutterEngine: " + this.flutterEngine);
    if (!this.isAttachedToFlutterEngine()) {
      Log.d(TAG, "FlutterView not attached to an engine. Not detaching.");
      return;
    }
    this.flutterEngine?.getPlatformViewsController()?.detachFromView();
    this.context.getApplicationContext().off('environment', this.environmentCallbackId);

    const flutterRenderer: FlutterRenderer|undefined = this.flutterEngine?.getRenderer();
    flutterRenderer?.removeIsDisplayingFlutterUiListener(this.flutterUiDisplayListener);
    this.flutterEngine = null;
    this.keyboardManager = null;
  }

  onWindowCreated() {
    this.sendUserSettingsToFlutter();
  }

  sendUserSettingsToFlutter(): void {
    if (!this.isAttachedToFlutterEngine()) {
      return;
    }

    const mainWindow = FlutterManager.getInstance()
      .getWindowStage(FlutterManager.getInstance().getUIAbility(this.context))
      .getMainWindowSync();

    const mediaQuery = mainWindow.getUIContext().getMediaQuery();

    let darkMode = mediaQuery.matchMediaSync('(dark-mode: true)');
    let brightness = darkMode.matches ? PlatformBrightness.DARK : PlatformBrightness.LIGHT;
    let uiAbility = FlutterManager.getInstance().getUIAbility(this.context);

    let fontSizeScale = uiAbility.context.config.fontSizeScale ?? 1.0;
    this.flutterEngine
      ?.getSettingsChannel()
      .startMessage()
      .setAlwaysUse24HourFormat(I18n.System.is24HourClock())
      .setNativeSpellCheckServiceDefined(false)
      .setBrieflyShowPassword(false)
      .setPlatformBrightness(brightness)
      .setTextScaleFactor(fontSizeScale)
      .send();
  }

  onAreaChange(newArea: Area | null, setFullScreen: boolean = false) {
    const originalMetrics = this.viewportMetrics.clone();

    if (newArea != null) {
      this.viewportMetrics.physicalWidth = vp2px(newArea.width as number);
      this.viewportMetrics.physicalHeight = vp2px(newArea.height as number);
    }

    // 根据是否全屏显示，设置标题栏高度（若全屏，则及时规避）
    if (this.checkFullScreen && (setFullScreen || FlutterManager.getInstance().getFullScreenListener().useFullScreen())) { // 全屏显示
      this.viewportMetrics.physicalViewPaddingTop = this.systemAvoidArea?.topRect.height ?? 0;
      this.viewportMetrics.physicalViewPaddingBottom = this.navigationAvoidArea?.bottomRect.height ?? 0;
    } else { // 非全屏显示（保持规避效果）
      // 顶部状态栏和底部导航栏规避为0，无平滑过渡效果
      this.viewportMetrics.physicalViewPaddingTop = 0;
      this.viewportMetrics.physicalViewPaddingBottom = 0;
    }

    this.viewportMetrics.physicalViewPaddingLeft = this.systemAvoidArea?.leftRect.width ?? 0;
    this.viewportMetrics.physicalViewPaddingRight = this.systemAvoidArea?.rightRect.width ?? 0;

    this.onKeyboardAreaChange()
    this.onGestureAreaChange()
    if (!this.viewportMetrics.isEqual(originalMetrics)) {
      this.updateViewportMetrics()
    }
  }

  private onKeyboardAreaChange() {
    if (this.checkKeyboard) {
      const mainWindow = FlutterManager.getInstance()
        .getWindowStage(FlutterManager.getInstance().getUIAbility(this.context))
        .getMainWindowSync();
      let keyboardAvoidArea = mainWindow.getWindowAvoidArea(window.AvoidAreaType.TYPE_KEYBOARD);
      this.viewportMetrics.physicalViewInsetTop = keyboardAvoidArea?.topRect.height ?? 0
      this.viewportMetrics.physicalViewInsetLeft = keyboardAvoidArea?.leftRect.width ?? 0
      this.viewportMetrics.physicalViewInsetBottom = keyboardAvoidArea?.bottomRect.height ?? 0
      this.viewportMetrics.physicalViewInsetRight = keyboardAvoidArea?.rightRect.width ?? 0
    } else {
      this.viewportMetrics.physicalViewInsetTop = 0
      this.viewportMetrics.physicalViewInsetLeft = 0
      this.viewportMetrics.physicalViewInsetBottom = 0
      this.viewportMetrics.physicalViewInsetRight = 0
    }
  }

  private onGestureAreaChange() {
    if (this.checkGesture) {
      const mainWindow = FlutterManager.getInstance()
        .getWindowStage(FlutterManager.getInstance().getUIAbility(this.context))
        .getMainWindowSync();
      let gestureAvoidArea = mainWindow.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM_GESTURE);
      this.viewportMetrics.systemGestureInsetTop = gestureAvoidArea?.topRect.height ?? 0
      this.viewportMetrics.systemGestureInsetLeft = gestureAvoidArea?.leftRect.width ?? 0
      this.viewportMetrics.systemGestureInsetBottom = gestureAvoidArea?.bottomRect.height ?? 0
      this.viewportMetrics.systemGestureInsetRight = gestureAvoidArea?.rightRect.width ?? 0
    } else {
      this.viewportMetrics.systemGestureInsetTop = 0
      this.viewportMetrics.systemGestureInsetLeft = 0
      this.viewportMetrics.systemGestureInsetBottom = 0
      this.viewportMetrics.systemGestureInsetRight = 0
    }
  }

  public isAttachedToFlutterEngine(): boolean {
    return this.flutterEngine != null
  }

  private updateViewportMetrics() {
    if (this.isAttachedToFlutterEngine()) {
      this?.flutterEngine?.getFlutterNAPI()?.setViewportMetrics(
        this.viewportMetrics.devicePixelRatio,
        this.viewportMetrics.physicalWidth,
        this.viewportMetrics.physicalHeight,
        this.viewportMetrics.physicalViewPaddingTop,
        this.viewportMetrics.physicalViewPaddingRight,
        this.viewportMetrics.physicalViewPaddingBottom,
        this.viewportMetrics.physicalViewPaddingLeft,
        this.viewportMetrics.physicalViewInsetTop,
        this.viewportMetrics.physicalViewInsetRight,
        this.viewportMetrics.physicalViewInsetBottom,
        this.viewportMetrics.physicalViewInsetLeft,
        this.viewportMetrics.systemGestureInsetTop,
        this.viewportMetrics.systemGestureInsetRight,
        this.viewportMetrics.systemGestureInsetBottom,
        this.viewportMetrics.systemGestureInsetLeft,
        this.viewportMetrics.physicalTouchSlop,
        new Array(0),
        new Array(0),
        new Array(0))
    }
  }

  onKeyEvent(event: KeyEvent) {
    this.keyboardManager?.handleKeyEvent(event)
  }

  onTouchEvent(event: TouchEvent) {
    this.ohosTouchProcessor?.onTouchEvent(event)
  }

  hasRenderedFirstFrame(): boolean {
    return this.isFlutterUiDisplayed;
  }

  /**
   * Adds the given {@code listener} to this {@code FlutterView}, to be notified upon Flutter's
   * first rendered frame.
   */
  addOnFirstFrameRenderedListener(listener: FlutterUiDisplayListener) {
    this.flutterUiDisplayListeners.add(listener);
  }

  /**
   * Adds the given {@code listener} to this {@code FlutterView}, to be notified upon Flutter's
   * first rendered frame.
   */
  removeOnFirstFrameRenderedListener(listener: FlutterUiDisplayListener) {
    this.flutterUiDisplayListeners.delete(listener);
  }

  setCheckFullScreen(check: boolean) {
    this.checkFullScreen = check;
  }

  setCheckKeyboard(check: boolean) {
    this.checkKeyboard = check
  }

  setCheckGesture(check: boolean) {
    this.checkGesture = check
  }

}
