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
import { FlutterRenderer } from '../engine/renderer/FlutterRenderer';
import { TouchEventId, TouchEventTracker } from './TouchEventTracker';

export enum PointerChange {
  CANCEL = 0,
  ADD = 1,
  REMOVE = 2,
  HOVER = 3,
  DOWN = 4,
  MOVE = 5,
  UP = 6,
  PAN_ZOOM_START = 7,
  PAN_ZOOM_UPDATE = 8,
  PAN_ZOOM_END = 9
}

export enum PointerDeviceKind {
  TOUCH = 0,
  MOUSE = 1,
  STYLUS = 2,
  INVERTED_STYLUS = 3,
  TRACKPAD = 4,
  UNKNOWN = 5
}

export enum PointerSignalKind {
  NONE = 0,
  SCROLL = 1,
  SCROLL_INERTIA_CANCEL = 2,
  SCALE = 3,
  UNKNOWN = 4
}

function getPointerChangeForTouchType(type: TouchType): number {
  switch (type) {
    case TouchType.Cancel:
      return PointerChange.CANCEL;
    case TouchType.Down:
      return PointerChange.DOWN;
    case TouchType.Move:
      return PointerChange.MOVE;
    case TouchType.Up:
      return PointerChange.UP;
    default:
        return -1;
  }
}

function getPointerChangeForMouseAction(action: MouseAction): number {
  switch (action) {
    case MouseAction.Hover:
      return PointerChange.HOVER;
    case MouseAction.Press:
      return PointerChange.DOWN;
    case MouseAction.Move:
      return PointerChange.MOVE;
    case MouseAction.Release:
      return PointerChange.REMOVE;
    default:
      return -1;
  }
}

function getPointerDeviceKindForSourceTool(sourceTool: SourceTool): number {
  switch (sourceTool) {
    case SourceTool.Finger:
      return PointerDeviceKind.TOUCH;
    case SourceTool.MOUSE:
      return PointerDeviceKind.MOUSE;
    case SourceTool.Pen:
      return PointerDeviceKind.STYLUS;
    default:
      return PointerDeviceKind.TOUCH;
  }
}

function getPointerChangeForPanZoom(pointerChange: PointerChange): number {
  switch (pointerChange) {
    case PointerChange.DOWN:
      return PointerChange.PAN_ZOOM_START;
    case PointerChange.MOVE:
      return PointerChange.PAN_ZOOM_UPDATE;
    case PointerChange.CANCEL:
    case PointerChange.UP:
      return PointerChange.PAN_ZOOM_END;
    default:
      return -1;
  }
}

const TAG = 'OhosTouchProcessor';

export default class OhosTouchProcessor {
  // This value must match kPointerDataFieldCount in pointer_data.cc. (The
  // pointer_data.cc also lists other locations that must be kept consistent.)
  private static POINTER_DATA_FIELD_COUNT: number = 36;
  private static BYTES_PER_FIELD: number = 8;

  // This value must match the value in framework's platform_view.dart.
  // This flag indicates whether the original Android pointer events were batched together.
  private static POINTER_DATA_FLAG_BATCHED: number = 1;

  // The view ID for the only view in a single-view Flutter app.
  private static IMPLICIT_VIEW_ID: number = 0;

  private static devicePixelRatio: number = display.getDefaultDisplaySync().densityPixels;

  private renderer: FlutterRenderer;
  private touchEventTracker: TouchEventTracker;

  private trackTouchEvents: boolean;
  private ongoingPans: Map<number, Array<number>> = new Map();

  constructor(renderer: FlutterRenderer,trackTouchEvents: boolean) {
    this.renderer = renderer;
    this.touchEventTracker = TouchEventTracker.getInstance();
    this.trackTouchEvents = trackTouchEvents;
  }

  public onTouchEvent(event: TouchEvent): void {
    const updateForSinglePointer = event.type === TouchType.Down || event.type === TouchType.Up;
    const touchCount = updateForSinglePointer ? 1 : event.touches.length;
    const buffer:ArrayBuffer = new ArrayBuffer(OhosTouchProcessor.POINTER_DATA_FIELD_COUNT * touchCount * OhosTouchProcessor.BYTES_PER_FIELD);
    const packet = new DataView(buffer);
    const pointerChange = getPointerChangeForTouchType(event.type);
    let offset = 0;
    if (updateForSinglePointer) {
      offset = this.addPointerForIndex(event, event.changedTouches[0], pointerChange, 0, packet, offset);
    } else {
      // Action.MOVE may not actually mean all pointers have moved
      // but it's the responsibility of a later part of the system to
      // ignore 0-deltas if desired.
      for (let p = 0; p < touchCount; p++) {
        offset = this.addPointerForIndex(event, event.touches[p], pointerChange, 0, packet, offset);
      }
    }

    // Verify that the packet is the expected size.
    if (packet.byteLength % (OhosTouchProcessor.POINTER_DATA_FIELD_COUNT * OhosTouchProcessor.BYTES_PER_FIELD) != 0) {
      throw new Error("Packet position is not on field boundary");
    }

    // Send the packet to Flutter.
    this.renderer.dispatchPointerDataPacket(packet.buffer, offset);
  }

  private addPointerForIndex(
    event: BaseEvent,
    touch: TouchObject,
    pointerChange: number,
    pointerData: number,
    packet: DataView,
    offset: number,
  ): number {
    if (pointerChange === -1) {
      return offset;
    }
    // TODO(dkwingsmt): Use the correct source view ID once Android supports
    // multiple views.
    // https://github.com/flutter/flutter/issues/134405
    const viewId:number = OhosTouchProcessor.IMPLICIT_VIEW_ID;
    const pointerId:number = touch.id;
    const pointerKind:number = getPointerDeviceKindForSourceTool(event.sourceTool);
    const viewToScreenCoords: Array<number> = [touch.x * OhosTouchProcessor.devicePixelRatio, touch.y * OhosTouchProcessor.devicePixelRatio];
    let buttons: number = 0;
    if (pointerKind === PointerDeviceKind.MOUSE) {
      if (pointerChange === PointerChange.DOWN && event.source === SourceType.Mouse) {
        this.ongoingPans.set(pointerId, viewToScreenCoords);
      }
    } else if (pointerKind === PointerDeviceKind.TOUCH) {
      if (pointerChange === PointerChange.DOWN || pointerChange === PointerChange.MOVE) {
        buttons = 1;
      }
    }

    let panZoomType: number = -1;
    const isTrackpadPan: boolean = this.ongoingPans.has(pointerId);
    if (isTrackpadPan) {
      panZoomType = getPointerChangeForPanZoom(pointerChange);
      if (panZoomType === -1) {
        return offset;
      }
    }

    let touchEventId: number = 0;
    if (this.trackTouchEvents) {
      const trackedEvent: TouchEventId = this.touchEventTracker.track(event);
      touchEventId = trackedEvent.getId();
    }
    const signalKind: number = PointerSignalKind.NONE;
    const timestamp: number = Math.round(event.timestamp / 1000);

    packet.setBigInt64(offset, BigInt(touchEventId), true); // event_id
    offset += 8;
    packet.setBigInt64(offset, BigInt(timestamp), true); // time_stamp
    offset += 8;
    if (isTrackpadPan) {
      packet.setBigInt64(offset, BigInt(panZoomType), true); //change
      offset += 8;
      packet.setBigInt64(offset, BigInt(PointerDeviceKind.TRACKPAD), true); // kind
      offset += 8;
    } else {
      packet.setBigInt64(offset, BigInt(pointerChange), true); // change
      offset += 8;
      packet.setBigInt64(offset, BigInt(pointerKind), true); // kind
      offset += 8;
    }
    packet.setBigInt64(offset, BigInt(signalKind), true); // signal_kind
    offset += 8;
    packet.setBigInt64(offset, BigInt(pointerId), true); // device
    offset += 8;
    packet.setBigInt64(offset, BigInt(0), true); // pointer_identifier, will be generated in pointer_data_packet_converter.cc.
    offset += 8;

    if (isTrackpadPan) {
      const panStart:Array<number> = this.ongoingPans.get(pointerId)!;
      packet.setFloat64(offset, panStart[0], true); // physical_x
      offset += 8;
      packet.setFloat64(offset, panStart[1], true); // physical_y
      offset += 8;
    } else {
      packet.setFloat64(offset, viewToScreenCoords[0], true); // physical_x
      offset += 8;
      packet.setFloat64(offset, viewToScreenCoords[1], true); // physical_y
      offset += 8;
    }
    packet.setFloat64(offset, 0.0, true); // physical_delta_x, will be generated in pointer_data_packet_converter.cc.
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // physical_delta_y, will be generated in pointer_data_packet_converter.cc.
    offset += 8;

    packet.setBigInt64(offset, BigInt(buttons), true); // buttons
    offset += 8;

    packet.setBigInt64(offset, BigInt(0), true); // obscured
    offset += 8;

    packet.setBigInt64(offset, BigInt(0), true); // synthesized
    offset += 8;

    packet.setFloat64(offset, event.pressure, true); // pressure
    offset += 8;

    packet.setFloat64(offset,  0.0, true); // pressure_min
    offset += 8;
    packet.setFloat64(offset, 1.0, true); // pressure_max
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // distance
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // distance_max
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // size
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // radius_major
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // radius_minor
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // radius_min
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // radius_max
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // orientation
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // tilt
    offset += 8;

    packet.setBigInt64(offset, BigInt(pointerData), true); // platformData
    offset += 8;

    packet.setFloat64(offset, 0.0, true); // scroll_delta_x
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // scroll_delta_y
    offset += 8;

    if (isTrackpadPan) {
      const panStart:Array<number> = this.ongoingPans.get(pointerId)!;
      packet.setFloat64(offset, viewToScreenCoords[0] - panStart[0], true); // pan_x
      offset += 8;
      packet.setFloat64(offset, viewToScreenCoords[1] - panStart[1], true); // pan_y
      offset += 8;
    } else {
      packet.setFloat64(offset, 0.0, true); // pan_x
      offset += 8;
      packet.setFloat64(offset, 0.0, true); // pan_y
      offset += 8;
    }
    packet.setFloat64(offset, 0.0, true); // pan_delta_x
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // pan_delta_y
    offset += 8;

    packet.setFloat64(offset, 1.0, true); // scale
    offset += 8;
    packet.setFloat64(offset, 0.0, true); // rotation
    offset += 8;
    packet.setBigInt64(offset, BigInt(viewId), true); // view_id
    offset += 8;

    if (isTrackpadPan && (panZoomType === PointerChange.PAN_ZOOM_END)) {
      this.ongoingPans.delete(pointerId);
    }

    return offset;

  }

}
