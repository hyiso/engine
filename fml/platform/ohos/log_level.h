/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#ifndef FLUTTER_FML_PLATFORM_OHOS_LOG_LEVEL_H_
#define FLUTTER_FML_PLATFORM_OHOS_LOG_LEVEL_H_

#include <hilog/log.h>

typedef enum {
  /** Debug level to be used by {@link OH_LOG_DEBUG} */
  HILOG_LOG_DEBUG = 3,
  /** Informational level to be used by {@link OH_LOG_INFO} */
  HILOG_LOG_INFO = 4,
  /** Warning level to be used by {@link OH_LOG_WARN} */
  HILOG_LOG_WARN = 5,
  /** Error level to be used by {@link OH_LOG_ERROR} */
  HILOG_LOG_ERROR = 6,
  /** Fatal level to be used by {@link OH_LOG_FATAL} */
  HILOG_LOG_FATAL = 7,
} ohos_LogLevel;

#endif  // FLUTTER_FML_PLATFORM_OHOS_LOG_LEVEL_H_

// HILOG_H
