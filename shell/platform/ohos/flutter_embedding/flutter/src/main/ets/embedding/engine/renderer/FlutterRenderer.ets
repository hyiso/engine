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
import { SurfaceTextureEntry, TextureRegistry } from '../../../view/TextureRegistry';
import FlutterNAPI from '../FlutterNAPI';
import { SurfaceTextureWrapper } from './SurfaceTextureWrapper.ets';
import Log from '../../../util/Log';
import { FlutterUiDisplayListener } from './FlutterUiDisplayListener';

const TAG = "FlutterRenderer"
export class FlutterRenderer implements TextureRegistry {
  private nextTextureId: number = 0;
  private flutterNAPI: FlutterNAPI;
  private _isDisplayingFlutterUi: boolean = false;
  private flutterUiDisplayListener: FlutterUiDisplayListener = Object({
    onFlutterUiDisplayed: () => {
      this._isDisplayingFlutterUi = true;
    },
    onFlutterUiNoLongerDisplayed: () => {
      this._isDisplayingFlutterUi = false;
    }
  });

  constructor(flutterNAPI: FlutterNAPI) {
    this.flutterNAPI = flutterNAPI;
    this.flutterNAPI.addIsDisplayingFlutterUiListener(this.flutterUiDisplayListener);
  }

  /**
   * Returns true if this {@code FlutterRenderer} is painting pixels to an HarmonyOS ArkUI
   * hierarchy, false otherwise.
   */
  isDisplayingFlutterUi(): boolean {
    return this._isDisplayingFlutterUi;
  }

  /**
   * Adds a listener that is invoked whenever this {@code FlutterRenderer} starts and stops painting
   * pixels to an HarmonyOS ArkUI hierarchy.
   */
  addIsDisplayingFlutterUiListener(listener: FlutterUiDisplayListener): void {
    this.flutterNAPI.addIsDisplayingFlutterUiListener(listener);
    if (this._isDisplayingFlutterUi) {
      listener.onFlutterUiDisplayed();
    }
  }

  /**
   * Removes a listener added via {@link
  * #addIsDisplayingFlutterUiListener(FlutterUiDisplayListener)}.
  */
  removeIsDisplayingFlutterUiListener(listener: FlutterUiDisplayListener): void {
    this.flutterNAPI.removeIsDisplayingFlutterUiListener(listener);
  }

  createSurfaceTexture(): SurfaceTextureEntry {
    const surfaceTexture: flutter.SurfaceTexture = new flutter.SurfaceTexture(0);
    const textureId:number = this.nextTextureId++;
    this.flutterNAPI.registerSurfaceTexture(textureId, surfaceTexture);
    return new SurfaceTextureRegistryEntry(this.flutterNAPI, textureId, surfaceTexture);
  }

  getTextureId(): number {
    this.nextTextureId++;
    return this.nextTextureId;
  }

  registerTexture(textureId: number): SurfaceTextureEntry {
    let surfaceId = this.flutterNAPI.registerTexture(textureId);
    return new SurfaceTextureEntryImpl(this.flutterNAPI, textureId, surfaceId);
  }

  setTextureBufferSize(textureId: number, width: number, height: number): void {
    this.flutterNAPI.setTextureBufferSize(textureId, width, height);
  }

  unregisterTexture(textureId: number): void {
    this.flutterNAPI.unregisterTexture(textureId);
  }

  onTrimMemory(level: number) {
    throw new Error('Method not implemented.');
  }

  dispatchPointerDataPacket(data: ArrayBuffer, position: number): void {
    this.flutterNAPI.dispatchPointerDataPacket(data, position);
  }
}

class SurfaceTextureRegistryEntry implements SurfaceTextureEntry {
  private flutterNAPI: FlutterNAPI;
  private id: number;
  private textureWrapper: SurfaceTextureWrapper;
  private released: boolean = false;

  constructor(flutterNAPI: FlutterNAPI, id: number, surfaceTexture: flutter.SurfaceTexture) {
    this.flutterNAPI = flutterNAPI;
    this.id = id;
    this.textureWrapper = new SurfaceTextureWrapper(surfaceTexture);
  }

  getTextureId(): number {
    return this.id;
  }

  getSurfaceId(): number {
    return this.textureWrapper.surfaceTexture().getSurfaceId();
  }

  surfaceTexture(): flutter.SurfaceTexture|undefined {
    return this.textureWrapper.surfaceTexture();
  }

  release() {
    if (!this.released) {
      Log.d(TAG, "Releasing a SurfaceTexture (" + this.id + ").");
      this.released = true;
      this.textureWrapper.release();
      this.flutterNAPI.unregisterTexture(this.id);
    }
  }
}

class SurfaceTextureEntryImpl implements SurfaceTextureEntry {
  private flutterNAPI: FlutterNAPI;
  private id: number;
  private surfaceId: number;
  private released: boolean = false;

  constructor(flutterNAPI: FlutterNAPI, id: number, surfaceId: number) {
    this.flutterNAPI = flutterNAPI;
    this.id = id;
    this.surfaceId = surfaceId;
  }

  getTextureId(): number {
    return this.id;
  }

  getSurfaceId(): number {
    return this.surfaceId;
  }

  surfaceTexture(): flutter.SurfaceTexture|undefined {
    return undefined;
  }

  release() {
    if (!this.released) {
      Log.d(TAG, "Releasing a SurfaceTexture (" + this.id + ").");
      this.released = true;
      this.flutterNAPI.unregisterTexture(this.id);
    }
  }
}