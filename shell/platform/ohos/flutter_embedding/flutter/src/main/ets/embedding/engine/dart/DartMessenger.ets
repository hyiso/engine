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
import { ErrorEvent, Queue, taskpool, worker, MessageEvents } from '@kit.ArkTS';

import Log from '../../../util/Log';
import { BinaryMessageHandler, BinaryMessenger, BinaryReply, TaskPriority, TaskQueue, TaskQueueOptions } from '../../../plugin/common/BinaryMessenger';
import FlutterNAPI from '../FlutterNAPI';
import { PlatformMessageHandler } from './PlatformMessageHandler';
import { TraceSection } from '../../../util/TraceSection';
import SendableBinaryMessageHandler from '../../../plugin/common/SendableBinaryMessageHandler'

/**
 * Message conduit for 2-way communication between Android and Dart.
 *
 * <p>See {@link BinaryMessenger}, which sends messages from Android to Dart
 *
 * <p>See {@link PlatformMessageHandler}, which handles messages to Android from Dart
 */

const TAG = "DartMessenger";

export class DartMessenger implements BinaryMessenger, PlatformMessageHandler {

  flutterNAPI: FlutterNAPI;

  /**
   * Maps a channel name to an object that contains the task queue and the handler associated with
   * the channel.
   */
  messageHandlers: Map<string, HandlerInfo> = new Map();

  bufferedMessages: Map<string, Array<BufferedMessageInfo>> = new Map();

  private bufferingIncomingMessagesEnabled: boolean = false;

  pendingReplies: Map<number, BinaryReply> = new Map();
  nextReplyId: number = 1;
  taskQueueFactory: TaskQueueFactory;
  createdTaskQueues: Map<TaskQueue, DartMessengerTaskQueue> = new Map();

  constructor(flutterNAPI: FlutterNAPI) {
    this.flutterNAPI = flutterNAPI;
    this.taskQueueFactory = new DefaultTaskQueueFactory();
  }

  makeBackgroundTaskQueue(options?: TaskQueueOptions): TaskQueue {
    let taskQueue: DartMessengerTaskQueue =
        this.taskQueueFactory.makeBackgroundTaskQueue(options ?? new TaskQueueOptions());
    let token: TaskQueueToken = new TaskQueueToken();
    this.createdTaskQueues.set(token, taskQueue);
    return token;
  }

  setMessageHandler(
      channel: string,
      handler: BinaryMessageHandler | SendableBinaryMessageHandler | null,
      taskQueue?: TaskQueue): void {
    if (handler == null) {
      Log.d(TAG, "Removing handler for channel '" + channel + "'");
      this.messageHandlers.delete(channel);
      return;
    }
    let dartMessengerTaskQueue: DartMessengerTaskQueue | undefined;
    if(taskQueue) {
      dartMessengerTaskQueue = this.createdTaskQueues.get(taskQueue);
      if(dartMessengerTaskQueue == null) {
        throw new Error(
            "Unrecognized TaskQueue, use BinaryMessenger to create your TaskQueue (ex makeBackgroundTaskQueue)."
          );
      }
    }
    Log.d(TAG, "Setting handler for channel '" + channel + "'");
    let list: Array<BufferedMessageInfo>|undefined;
    this.messageHandlers.set(channel, new HandlerInfo(handler, dartMessengerTaskQueue));
    if (this.bufferedMessages.has(channel)) {
      list = this.bufferedMessages.get(channel);
      this.bufferedMessages.delete(channel);
    }
    if (list) {
      for (let info of list) {
        this.dispatchMessageToQueue(channel, this.messageHandlers.get(channel), info.message, info.replyId, info.messageData);
      }
    }
  }

  enableBufferingIncomingMessages(): void {
    this.bufferingIncomingMessagesEnabled = true;
  }

  disableBufferingIncomingMessages(): void {
    this.bufferingIncomingMessagesEnabled = false;
    const pendingMessages = this.bufferedMessages;
    this.bufferedMessages = new Map();
    pendingMessages.forEach((list, channel) => {
      for (let info of list) {
        this.dispatchMessageToQueue(channel, this.messageHandlers.get(channel), info.message, info.replyId, info.messageData);
      }
    });
  }

  send(channel: String, message: ArrayBuffer, callback?: BinaryReply): void {
    let traceId: number = TraceSection.begin("DartMessenger#send on " + channel);
    try {
      Log.d(TAG, "Sending message with callback over channel '" + channel + "'");
      let replyId: number = this.nextReplyId++;
      if (callback != null) {
        this.pendingReplies.set(replyId, callback);
      }
      if (message == null) {
        this.flutterNAPI.dispatchEmptyPlatformMessage(channel, replyId);
      } else {
        this.flutterNAPI.dispatchPlatformMessage(channel, message, message.byteLength, replyId);
      }
    } finally {
      TraceSection.endWithId("DartMessenger#send on " + channel, traceId);
    }
  }

  invokeHandler(handlerInfo: HandlerInfo | null, message: ArrayBuffer, replyId: number): void {
    if (handlerInfo != null) {
      try {
        Log.d(TAG, "Deferring to registered handler to process message.");
        handlerInfo.handler.onMessage(message, new Reply(this.flutterNAPI, replyId));
      } catch (ex) {
        Log.e(TAG, "Uncaught exception in binary message listener", ex);
        this.flutterNAPI.invokePlatformMessageEmptyResponseCallback(replyId);
      }
    } else {
      Log.d(TAG, "No registered handler for message. Responding to Dart with empty reply message.");
      this.flutterNAPI.invokePlatformMessageEmptyResponseCallback(replyId);
    }
  }

  dispatchMessageToQueue(
      channel: string,
      handlerInfo: HandlerInfo|undefined,
      message: ArrayBuffer,
      replyId: number,
      messageData: number): void {
    const taskQueue = handlerInfo?.taskQueue;
    TraceSection.beginWithId("PlatformChannel ScheduleHandler on" + channel, replyId);
    let taskState: TaskState = new TaskState(handlerInfo?.handler as ESObject, message);
    const callback = () => {
      TraceSection.endWithId("PlatformChannel ScheduleHandler on" + channel, replyId);
      const traceId = TraceSection.begin("DartMessenger#handleMessageFromDart on " + channel);
      this.invokeHandler(handlerInfo!, message, replyId);
      this.flutterNAPI.cleanupMessageData(messageData);
      TraceSection.endWithId("DartMessenger#handleMessageFromDart on " + channel, traceId);
    };
    if (taskQueue == null) {
      callback();
    } else {
      taskQueue?.dispatch(taskState, new Reply(this.flutterNAPI, replyId), );
    }
  }

  handleMessageFromDart(channel: string, message: ArrayBuffer, replyId: number, messageData: number): void {
    Log.d(TAG, "Received message from Dart over channel '" + channel + "'");
    let handlerInfo = this.messageHandlers.get(channel);
    const messageDeffered = this.bufferingIncomingMessagesEnabled && !handlerInfo;
    if (messageDeffered) {
      let list: Array<BufferedMessageInfo> = this.bufferedMessages.get(channel) ?? [];
      list.push(new BufferedMessageInfo(message, replyId, messageData));
      this.bufferedMessages.set(channel, list);
    } else {
      this.dispatchMessageToQueue(channel, handlerInfo, message, replyId, messageData);
    }
  }

  handlePlatformMessageResponse(replyId: number, reply: ArrayBuffer): void {
    Log.d(TAG, "Received message reply from Dart.");
    let callback: BinaryReply | null = this.pendingReplies.get(replyId) ?? null;
    this.pendingReplies.delete(replyId);
    if (callback != null) {
      try {
        Log.d(TAG, "Invoking registered callback for reply from Dart.");
        callback.reply(reply);
      } catch (e) {
        Log.e(TAG, "Uncaught exception in binary message reply handler", e);
      }
    }
  }

  /**
   * Returns the number of pending channel callback replies.
   *
   * <p>When sending messages to the Flutter application using {@link BinaryMessenger#send(String,
   * ByteBuffer, io.flutter.plugin.common.BinaryMessenger.BinaryReply)}, developers can optionally
   * specify a reply callback if they expect a reply from the Flutter application.
   *
   * <p>This method tracks all the pending callbacks that are waiting for response, and is supposed
   * to be called from the main thread (as other methods). Calling from a different thread could
   * possibly capture an indeterministic internal state, so don't do it.
   */
  getPendingChannelResponseCount(): number {
    return this.pendingReplies.size;
  }
}

/**
 * Holds information about a platform handler, such as the task queue that processes messages from
 * Dart.
 */
class HandlerInfo {
  handler: BinaryMessageHandler | SendableBinaryMessageHandler;
  taskQueue: DartMessengerTaskQueue | undefined;

  constructor(handler: BinaryMessageHandler | SendableBinaryMessageHandler,
              taskQueue: DartMessengerTaskQueue | undefined) {
    this.handler = handler;
    this.taskQueue = taskQueue;
  }
}

class BufferedMessageInfo {
  message: ArrayBuffer;
  replyId: number;
  messageData: number;

  constructor(message: ArrayBuffer, replyId: number, messageData: number) {
    this.message = message;
    this.replyId = replyId;
    this.messageData = messageData;
  }
}

class Reply implements BinaryReply {
  flutterNAPI: FlutterNAPI;
  replyId: number;
  done: boolean = false;

  constructor(flutterNAPI: FlutterNAPI, replyId: number) {
    this.flutterNAPI = flutterNAPI;
    this.replyId = replyId;
  }

  reply(reply: ArrayBuffer | null) {
    if (this.done) {
      throw new Error("Reply already submitted");
    }

    if (reply == null) {
      this.flutterNAPI.invokePlatformMessageEmptyResponseCallback(this.replyId);
    } else {
      this.flutterNAPI.invokePlatformMessageResponseCallback(this.replyId, reply, reply.byteLength);
    }
  }
}

export class TaskState {
  handler: SendableBinaryMessageHandler;
  message: ArrayBuffer;

  constructor(
      handler: SendableBinaryMessageHandler,
      message: ArrayBuffer) {
    this.handler = handler;
    this.message = message;
  }
}

interface DartMessengerTaskQueue {
  dispatch(taskState: TaskState, reply: Reply, callback?: () => void): void;
}

interface SerialTaskQueue extends DartMessengerTaskQueue {
}

interface TaskQueueFactory {
  makeBackgroundTaskQueue(options: TaskQueueOptions): DartMessengerTaskQueue;
}

class ConcurrentTaskQueue implements DartMessengerTaskQueue {
  private priority: TaskPriority;

  constructor(priority: TaskPriority) {
    this.priority = priority;
  }

  dispatch(taskState: TaskState, reply: Reply, callback?: () => void): void {
    let task: taskpool.Task = new taskpool.Task(handleMessageInBackground,
                                                taskState.handler,
                                                taskState.message);
    taskpool.execute(task, this.priority as number).then((result: Object) => {
      reply.reply(result as ArrayBuffer);
      callback?.();
    }).catch((err: string) => {
      reply.reply(null);
      callback?.();
      Log.e(TAG, "Oops! Failed to execute task: ", err);
    });
  }
}

const scriptURL: string = '../workers/PlatformChannelWorker.ets';
class SerialTaskQueueWithWorker implements SerialTaskQueue {
  private static workerInstance: worker.ThreadWorker | null = null;

  constructor () {
    if (!SerialTaskQueueWithWorker.workerInstance) {
      SerialTaskQueueWithWorker.workerInstance =
        new worker.ThreadWorker(scriptURL, {name: 'PlatformChannelWorker'});
    }
  }

  dispatch(taskState: TaskState, reply: Reply, callback?: () => void): void {
    SerialTaskQueueWithWorker.workerInstance!.onmessage = (e: MessageEvents): void => {
      reply.reply(e.data as ArrayBuffer);
      callback?.();
    }

    SerialTaskQueueWithWorker.workerInstance!.onerror = (err: ErrorEvent) => {
      reply.reply(null);
      callback?.();
      Log.e(TAG, "Oops! Failed to execute task in worker thread: ", err.message);
    }

    SerialTaskQueueWithWorker.workerInstance!.postMessageWithSharedSendable(taskState, [taskState.message]);
  }
}

type Runnable = () => Promise<void>;
class SerialTaskQueueWithTaskPool implements SerialTaskQueue {
  private priority: TaskPriority;
  private queue: Queue<Runnable> = new Queue();
  private isRunning: boolean = false;

  constructor(priority: TaskPriority) {
    this.priority = priority;
  }

  dispatch(taskState: TaskState, reply: Reply, callback?: () => void): void {
    let task: taskpool.Task = new taskpool.Task(handleMessageInBackground,
                                                taskState.handler,
                                                taskState.message);
    const runnable: Runnable = async () => {
      try {
        const result = await taskpool.execute(task, this.priority as number);
        reply.reply(result as ArrayBuffer);
        callback?.();
      } catch (err) {
        reply.reply(null);
        callback?.();
        Log.e(TAG, "Oops! Failed to execute task: ", err);
      }
    };

    this.queue.add(runnable);

    if (!this.isRunning) {
      this.runNext();
    }
  }

  private async runNext(): Promise<void> {
    if (this.queue.length > 0) {
      this.isRunning = true;
      const task = this.queue.pop();
      try {
        await task();
      } finally {
        this.isRunning = false;
        this.runNext(); // 执行下一个任务
      }
    }
  }
}

class DefaultTaskQueueFactory implements TaskQueueFactory {
  makeBackgroundTaskQueue(options: TaskQueueOptions): DartMessengerTaskQueue {
    if (options.isSingleThreadMode()) {
      return new SerialTaskQueueWithWorker();
    } else {
      if (options.getIsSerial()) {
        return new SerialTaskQueueWithTaskPool(options.getPriority());
      }
      return new ConcurrentTaskQueue(options.getPriority());
    }
  }
}

class TaskQueueToken implements TaskQueue {}

@Concurrent
async function handleMessageInBackground(
    handler: SendableBinaryMessageHandler,
    message: ArrayBuffer): Promise<ArrayBuffer | null> {
  const result = await new Promise<ArrayBuffer | null>((resolve, reject) => {
    try {
      handler.onMessage(message, {
        reply: (reply: ArrayBuffer | null): void => {
          resolve(reply);
        }
      });
    } catch (e) {
      reject(null);
      Log.e('WARNING', "Oops! Failed to handle message in the background: ", e);
    }
  });
  return result;
}
