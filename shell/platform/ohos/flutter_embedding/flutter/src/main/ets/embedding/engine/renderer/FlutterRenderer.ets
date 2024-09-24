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

import { SurfaceTextureEntry, TextureRegistry } from '../../../view/TextureRegistry';
import FlutterNapi from '../FlutterNapi';

const TAG = "FlutterRenderer"
export class FlutterRenderer implements TextureRegistry {
  private nextTextureId: number = 0;
  private flutterNapi: FlutterNapi;

  constructor(flutterNapi: FlutterNapi) {
    this.flutterNapi = flutterNapi;
  }

  createSurfaceTexture(): SurfaceTextureEntry {
    this.nextTextureId = this.nextTextureId + 1;
    return this.registerTexture(this.nextTextureId);
  }

  getTextureId(): number {
    this.nextTextureId = this.nextTextureId + 1;
    return this.nextTextureId;
  }

  registerTexture(textureId: number): SurfaceTextureEntry {
    let surfaceId = this.flutterNapi.registerTexture(textureId);
    return new SurfaceTextureRegistryEntry(this.flutterNapi, textureId, surfaceId);
  }

  setTextureBufferSize(textureId: number, width: number, height: number): void {
    this.flutterNapi.setTextureBufferSize(textureId, width, height);
  }

  unregisterTexture(textureId: number): void {
    this.flutterNapi.unregisterTexture(textureId);
  }

  onTrimMemory(level: number) {
    throw new Error('Method not implemented.');
  }
}

export class SurfaceTextureRegistryEntry implements SurfaceTextureEntry {
  private textureId: number = 0;
  private surfaceId: number = 0;
  private flutterNapi: FlutterNapi;
  private released: boolean = false;

  constructor(flutterNapi:FlutterNapi, textureId: number, surfaceId: number) {
    this.flutterNapi = flutterNapi;
    this.textureId = textureId;
    this.surfaceId = surfaceId;
  }

  getTextureId(): number {
    return this.textureId;
  }

  getSurfaceId(): number {
    return this.surfaceId;
  }

  release() {
    if (!this.released) {
      this.flutterNapi.unregisterTexture(this.textureId);
      this.released = true;
    }
  }
}