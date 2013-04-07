/*
 * Copyright (C) 2005 The Android Open Source Project
 * Copyright (C) ST-Ericsson SA 2012
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* Enables Android logging in WebCore by re-defining all log macros
 * with the 'A' prefix. i.e. ALOGE instead of LOGE etc.
 * This should be the first file you include in your cpp file.
 * Don't for get to #define LOG_TAG "MyTag" before you include
 */
#ifndef ANDROIDLOGGING_H
#define ANDROIDLOGGING_H 1

#include <cutils/log.h>

#define ALOG_VERBOSE ANDROID_LOG_VERBOSE
#define ALOG_DEBUG   ANDROID_LOG_DEBUG
#define ALOG_INFO    ANDROID_LOG_INFO
#define ALOG_WARN    ANDROID_LOG_WARN
#define ALOG_ERROR   ANDROID_LOG_ERROR

#ifndef ALOGV
#ifndef ALOG_ENABLE_VERBOSE
#define ALOGV(...)   ((void)0)
#else
#define ALOGV(...) ((void)ALOG(ALOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif

#ifndef ALOGV_IF
#ifndef ALOG_ENABLE_VERBOSE
#define ALOGV_IF(cond, ...)   ((void)0)
#else
#define ALOGV_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(ALOG_VERBOSE, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
#endif

#ifndef ALOGD
#ifndef ALOG_ENABLE_DEBUG
#define ALOGD(...)   ((void)0)
#else
#define ALOGD(...) ((void)ALOG(ALOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif
#endif

#ifndef ALOGD_IF
#ifndef ALOG_ENABLE_DEBUG
#define ALOGD_IF(cond, ...)   ((void)0)
#else
#define ALOGD_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(ALOG_DEBUG, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
#endif

#ifndef ALOGI
#define ALOGI(...) ((void)ALOG(ALOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGI_IF
#define ALOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(ALOG_INFO, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

#ifndef ALOGW
#define ALOGW(...) ((void)ALOG(ALOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGW_IF
#define ALOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(ALOG_WARN, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

#ifndef ALOGE
#define ALOGE(...) ((void)ALOG(ALOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGE_IF
#define ALOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(ALOG_ERROR, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

#ifndef ALOG
#define ALOG(priority, tag, ...) \
    android_printLog(priority, tag, __VA_ARGS__)
#endif

/* Undefine the regular macros */
#undef LOG
#undef LOGD
#undef LOGI
#undef LOGW
#undef LOGE

#endif        //  #ifndef ANDROIDLOGGING_H

