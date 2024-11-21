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

import { FlutterView } from '../../view/FlutterView';
import FlutterManager from './FlutterManager';
import { DVModel, DVModelChildren, DynamicView } from '../../view/DynamicView/dynamicView';

const TAG = "FlutterPage";

/**
 * 基础page组件，承载XComponent组件
 */
@Component
export struct FlutterPage {
  @Prop viewId: string = ""
  @Prop xComponentType: XComponentType = XComponentType.SURFACE

  @Builder doNothingBuilder() {}
  @BuilderParam splashScreenView: () => void = this.doNothingBuilder;
  @State showSplashScreen: boolean = true;

  @State checkFullScreen: boolean = true;
  @State checkKeyboard: boolean = true;
  @State checkGesture: boolean = true;

  @State rootDvModel: DVModelChildren | undefined = undefined

  private flutterView?: FlutterView | null
  private lastArea?: Area;

  aboutToAppear() {
    this.flutterView = FlutterManager.getInstance().getFlutterView(this.viewId);
    this.flutterView?.addFirstFrameListener(this)

    this.flutterView?.setCheckFullScreen(this.checkFullScreen)
    this.flutterView?.setCheckKeyboard(this.checkKeyboard)
    this.flutterView?.setCheckGesture(this.checkGesture)

    this.rootDvModel = this.flutterView!!.getDVModel().children
  }

  aboutToDisappear() {
    this.flutterView?.removeFirstFrameListener(this);
  }

  onFirstFrame() {
    this.showSplashScreen = false;
  }

  build() {
    Stack() {
      ForEach(this.rootDvModel!!, (child: ESObject) => {
        DynamicView({
          model: child as DVModel,
          params: child.params,
          events: child.events,
          children: child.children,
          customBuilder: child.builder
        })
      }, (child: ESObject) => `${child.id_}`)

      XComponent({
        id: this.viewId,
        type: this.xComponentType,
        controller: this.flutterView,
      })
        .id(this.viewId)
        .focusable(true)
        .backgroundColor(Color.Transparent)

      if (this.showSplashScreen) {
        this.splashScreenView();
      }
    }
    .defaultFocus(true)
    .onAreaChange((oldValue: Area, newValue: Area) => {
      if (!this.lastArea || oldValue.width != newValue.width
        || oldValue.height != newValue.height) {
        this.lastArea = newValue;
        this.flutterView?.onAreaChange(newValue)
      }
    })
    .onTouch((event: TouchEvent) => {
      this.flutterView?.onTouchEvent(event);
    })
    .onKeyPreIme((event: KeyEvent) => {
      this.flutterView?.onKeyEvent(event);
      return false;
    })
  }
}
