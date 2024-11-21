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

export interface TextureRegistry {

  createSurfaceTexture(): SurfaceTextureEntry;
  getTextureId(): number;
  registerTexture(textureId: number): SurfaceTextureEntry;
  unregisterTexture(textureId: number): void;
  onTrimMemory(level: number) : void;
}

/** An entry in the texture registry. */
export interface TextureEntry {
  /** @return The identity of this texture. */
  getTextureId(): number;

  /** De-registers and releases all resources . */
  release(): void;
}

export interface SurfaceTextureEntry extends TextureEntry {

  getSurfaceId(): number;

  surfaceTexture(): flutter.SurfaceTexture|undefined;
}

export interface OnFrameConsumedListener {
  onFrameConsumed(): void;
}

export interface OnTrimMemoryListener {
  onTrimMemory(level: number) : void;
}