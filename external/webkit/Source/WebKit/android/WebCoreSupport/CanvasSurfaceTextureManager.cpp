/*
 * Copyright 2012, The Android Open Source Project
 * Copyright (C) ST-Ericsson SA 2012
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG ("CanvasSurfaceTextureManager")
#include "AndroidLogging.h"

#include "config.h"
#include "CanvasSurfaceTextureManager.h"

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID)

#include "CanvasSurfaceTexture.h"
#include "WebViewCore.h"
#include "WebCoreJni.h"
#include "GLUtils.h"
#include <JNIHelp.h>
#include <JNIUtility.h>
#include <gui/SurfaceTexture.h>

#define SURFACE_TEXTURE_TIMEOUT (500 * 1000 * 1000) /* 500 ms in ns*/


// Forward decl
namespace android {
sp<SurfaceTexture> SurfaceTexture_getSurfaceTexture(JNIEnv* env, jobject thiz);

static void attachToJava(WebCore::CanvasSurfaceTextureManager::JavaGlue** glue,
                         WebCore::CanvasSurfaceTextureManager* self,
                         WebCore::FrameView* view,
                         jobject* canvasMgrInstance);

static void detachFromJava(jobject* canvasMgrInstance);
};

namespace WebCore {

struct CanvasSurfaceTextureManager::JavaGlue {
    jmethodID m_getInstance;
    jmethodID m_canvasElementCreated;
    jmethodID m_canvasElementDestroyed;
    jmethodID m_postSyncMessage;
};

struct CanvasSurfaceTextureInfo {
    CanvasSurfaceTexture*       m_cst;
    sp<android::SurfaceTexture> m_texture;
    GLuint                      m_textureId;
    jobject                     m_canvasMgrInstance;
    int                         m_canvasMgrMapKey;
    bool                        m_syncRequested;
    bool                        m_textureAssigned;
};

/* Holds information about a particular
 * HTML5CanvasManager instance on the java side
 * and its associated ref count */
struct CanvasManagerInfo {
    jobject                  m_canvasMgrInstance;
    unsigned int             m_refCnt;
    bool                     m_syncRequested;
};

CanvasSurfaceTextureManager::CanvasSurfaceTextureManager() : m_javaGlue(0), m_current(-1)
{
    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);
}

CanvasSurfaceTextureManager::~CanvasSurfaceTextureManager()
{
    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

    /* TODO: Cleanup */
    if (m_canvasSurfaceTextureInfoMap.size() > 0) {
        ALOGE("Destructor called:  m_canvasSurfaceTextureInfoMap.size() > 0 !!!");
    }

    if (m_canvasManagerMap.size() > 0) {
        ALOGE("Destructor called:  m_canvasManagerMap.size() > 0 !!!");
        // android::detachFromJava(&m_javaGlue);
    }

    delete m_javaGlue;
    m_javaGlue = 0;
}

void CanvasSurfaceTextureManager::registerCanvasSurfaceTexture(int id, CanvasSurfaceTexture* cst)
{
    android::Mutex::Autolock lock(m_mapLock);

    if (!cst)
        return;

    if (m_canvasSurfaceTextureInfoMap.get(id)) {
        ALOGE("%s: ID already in map!", __func__);
        return;
    }

    FrameView* view = cst->frameView();
    if (!view) {
        ALOGE("%s: FrameView was NULL, unable to contact the Java side", __func__);
        return;
    }

    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

    CanvasSurfaceTextureInfo* info = new CanvasSurfaceTextureInfo();
    info->m_cst = cst;
    info->m_canvasMgrMapKey = reinterpret_cast<int>(view);
    info->m_syncRequested = false;

    m_canvasSurfaceTextureInfoMap.add(id, info);

    CanvasManagerInfo* mgrInfo = m_canvasManagerMap.get(info->m_canvasMgrMapKey);
    if (!mgrInfo) {
        mgrInfo = new CanvasManagerInfo();
        mgrInfo->m_refCnt = 1;
        mgrInfo->m_syncRequested = false;

        android::attachToJava(&m_javaGlue, this, view, &mgrInfo->m_canvasMgrInstance);

        m_canvasManagerMap.add(info->m_canvasMgrMapKey, mgrInfo);
    } else {
        mgrInfo->m_refCnt++;
    }

    /* Save the java object in both maps */
    info->m_canvasMgrInstance = mgrInfo->m_canvasMgrInstance;

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (env) {
        env->CallVoidMethod(info->m_canvasMgrInstance,
                            m_javaGlue->m_canvasElementCreated, id);

        /* block until the SurfaceTexture is created */
        waitForSurfaceTexture(info);
    } else {
        ALOGW("%s: Could not acquire JNIEnv. Unable to call out to Java.", __func__);
    }

    ALOGD("%s: PGC: %d added.", __func__, id);

}

void CanvasSurfaceTextureManager::deregisterCanvasSurfaceTexture(int id)
{
    android::Mutex::Autolock lock(m_mapLock);

    CanvasSurfaceTextureInfo* info = m_canvasSurfaceTextureInfoMap.take(id);
    if (info) {
        // call java
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (env) {
            env->CallVoidMethod(info->m_canvasMgrInstance,
                                m_javaGlue->m_canvasElementDestroyed, id);
        } else {
            ALOGW("%s: Could not acquire JNIEnv. Unable to call out to Java.", __func__);
        }

        ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

        // Decrease the refcount the m_canvasManagerMap
        CanvasManagerInfo* mgrInfo = m_canvasManagerMap.get(info->m_canvasMgrMapKey);
        if (mgrInfo && (--mgrInfo->m_refCnt == 0)) {
            ALOGD("%s: Detaching from java instance: %p", __func__, mgrInfo->m_canvasMgrInstance);
            android::detachFromJava(&mgrInfo->m_canvasMgrInstance);
            m_canvasManagerMap.remove(info->m_canvasMgrMapKey);
            delete mgrInfo;
        }

        delete info;

        ALOGD("%s: PGC: %d removed.", __func__, id);
    } else {
        ALOGW("%s: Unable to deregister id %d not found in map!", __func__, id);
    }

}

void CanvasSurfaceTextureManager::requestSync(int id)
{
    android::Mutex::Autolock lock(m_mapLock);

    CanvasSurfaceTextureInfo* info = m_canvasSurfaceTextureInfoMap.get(id);

    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

    if (!info) {
        ALOGE("%s: id: %d not in map!", __func__, id);
        return;
    }

    if (info->m_syncRequested)
        return; // sync already requested.

    CanvasManagerInfo* mgrInfo = m_canvasManagerMap.get(info->m_canvasMgrMapKey);

    if (!mgrInfo) {
        ALOGE("Sync requested for id: %d, but mgrInfo was NULL");
        return;
    }

    if (!mgrInfo->m_syncRequested) {
        // Sync hasn't been requested for this HTML5CanvasManagerInstance
        // Do it now.
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (env) {
            env->CallVoidMethod(mgrInfo->m_canvasMgrInstance,
                                m_javaGlue->m_postSyncMessage, info->m_canvasMgrMapKey);
            mgrInfo->m_syncRequested = true;
        } else {
            ALOGW("%s: Could not acquire JNIEnv. Unable to call out to Java.", __func__);
            return;
        }

    }

    info->m_syncRequested = true;

}

void CanvasSurfaceTextureManager::ensureCurrent(int id, EGLDisplay display, EGLSurface surface, EGLContext context) {

    if (id != m_current) {
        ALOGD("ensureCurrent() setting current to %d", id);
        /* Flush the previously current context before switching */
        CanvasSurfaceTextureInfo* info = m_canvasSurfaceTextureInfoMap.get(m_current);
        if (info && info->m_cst && info->m_syncRequested) {
            info->m_cst->performSync();
            info->m_syncRequested = false;
        }

        EGLBoolean returnValue = eglMakeCurrent(display, surface, surface, context);
        GLUtils::checkEglError("eglMakeCurrent", returnValue);
        m_current = id;
    }

}

void CanvasSurfaceTextureManager::waitForSurfaceTexture(CanvasSurfaceTextureInfo* info)
{
    /* m_mapLock is already locked here */
    ALOGD("Wating for SurfaceTexture from UI Thread");
    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

    /* Wait max 500 ms to give the UI thread to send us a SurfaceTexture */
    bool timedOut = false;
    while (!info->m_textureAssigned && !timedOut) {
        int ret = m_mapCond.waitRelative(m_mapLock, SURFACE_TEXTURE_TIMEOUT);
        timedOut = (ret == android::TIMED_OUT);
    }

    if (timedOut) {
        ALOGE("Timeout while waiting for surface texture!");
        // TODO: Can we handle this?
        return;
    }

    ALOGD("Got surfaceTexture from UI thread. notifying WebCore thread.");

    // call context
    info->m_cst->setSurfaceTexture(info->m_texture, info->m_textureId);

}

/* Called from the UI thread */
bool CanvasSurfaceTextureManager::setSurfaceTextureForContext(int id,
                                      sp<android::SurfaceTexture> texture, GLuint textureId)
{
    android::Mutex::Autolock lock(m_mapLock);

    CanvasSurfaceTextureInfo* info = m_canvasSurfaceTextureInfoMap.get(id);
    if (info) {
        info->m_texture = texture;
        info->m_textureId = textureId;
        info->m_textureAssigned = true;

        ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);
        m_mapCond.signal();

        return true;
    } else {
        ALOGW("%s: id %d not found in map!", __func__, id);
        return false;
    }
}

void CanvasSurfaceTextureManager::performSync(int key)
{
    android::Mutex::Autolock lock(m_mapLock);

    CanvasManagerInfo* mgrInfo = m_canvasManagerMap.get(key);

    if (!mgrInfo) {
        ALOGE("%s(): key: %d not in map!!");
        return;
    }

    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

    mgrInfo->m_syncRequested = false;

    ContextIterator end = m_canvasSurfaceTextureInfoMap.end();
    for (ContextIterator it = m_canvasSurfaceTextureInfoMap.begin(); it != end; ++it) {
        if (!it->second->m_syncRequested || (it->second->m_canvasMgrMapKey != key))
            continue;

        if (it->second->m_cst)
            it->second->m_cst->performSync();

        it->second->m_syncRequested = false;
    }

}

void CanvasSurfaceTextureManager::setPaintingDisabled(int viewKey, bool disable)
{

    android::Mutex::Autolock lock(m_mapLock);

    ALOGD("%s: Thread: %d, this: %p", __func__, pthread_self(), this);

    ContextIterator end = m_canvasSurfaceTextureInfoMap.end();
    for (ContextIterator it = m_canvasSurfaceTextureInfoMap.begin(); it != end; ++it) {
        if ((it->second->m_canvasMgrMapKey != viewKey)) {
            continue;
        }

        if (it->second->m_cst)
            it->second->m_cst->disablePainting(disable);
    }
}


} // namespace WebCore

/* JNI entry points */
namespace android {

/* Our counterpart on the java side */
static const char* g_canvasManagerJavaClass = "android/webkit/HTML5CanvasManager";

static void attachToJava(WebCore::CanvasSurfaceTextureManager::JavaGlue** glue,
                         WebCore::CanvasSurfaceTextureManager* self,
                         WebCore::FrameView* view,
                         jobject* canvasMgrInstance) {

    if (!glue || !self || !view || !canvasMgrInstance)
        return;

    if (!(*glue)) {
        *glue = new WebCore::CanvasSurfaceTextureManager::JavaGlue();
    }

    if (!view)
        return;

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(g_canvasManagerJavaClass);

    if (!clazz)
        return;

    jobject obj = 0;

    AutoJObject javaObject = WebViewCore::getWebViewCore(view)->getJavaObject();
    if (!javaObject.get())
        return;

    (*glue)->m_getInstance = env->GetStaticMethodID(clazz,
                                                   "getInstance",
                                                   "(Landroid/webkit/WebViewCore;II)Landroid/webkit/HTML5CanvasManager;");

    (*glue)->m_canvasElementCreated = env->GetMethodID(clazz,
                                                      "canvasElementCreated",
                                                      "(I)V");

    (*glue)->m_canvasElementDestroyed = env->GetMethodID(clazz,
                                                        "canvasElementDestroyed",
                                                        "(I)V");

    (*glue)->m_postSyncMessage = env->GetMethodID(clazz,
                                                  "postSyncMessage",
                                                  "(I)V");

    // Get the HTML5CanvasManager instance
    obj = env->CallStaticObjectMethod(clazz, (*glue)->m_getInstance, javaObject.get(), self, reinterpret_cast<int>(view));
    *canvasMgrInstance = env->NewGlobalRef(obj);

    // Clean up.
    env->DeleteLocalRef(obj);
    env->DeleteLocalRef(clazz);
    checkException(env);

}

static void detachFromJava(jobject* canvasMgrInstance) {

    if (!canvasMgrInstance)
        return;

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    if (*canvasMgrInstance)
        env->DeleteGlobalRef(*canvasMgrInstance);

    *canvasMgrInstance = 0;

}

// Called on the WebCore thread, when the surface texture has been created
// for us on the UI Thread
static bool SendSurfaceTexture(JNIEnv* env, jobject obj, jobject surfTex,
                               int canvasSurTexMgr, int canvasLayerId,
                               int textureName)
{
    /* Sanity checks */
    sp<SurfaceTexture> texture = NULL;
    WebCore::CanvasSurfaceTextureManager* self =
                reinterpret_cast<WebCore::CanvasSurfaceTextureManager*>(canvasSurTexMgr);

    if (!self)
        return false;

    if(surfTex) {
        texture = android::SurfaceTexture_getSurfaceTexture(env, surfTex);
        if (!texture.get())
            return false;
    }

    return self->setSurfaceTextureForContext(canvasLayerId, texture, textureName);
}

static void PerformSync(JNIEnv* env, jobject obj, int canvasSurTexMgr, int userdata)
{

    WebCore::CanvasSurfaceTextureManager* self =
                reinterpret_cast<WebCore::CanvasSurfaceTextureManager*>(canvasSurTexMgr);
    if (!self)
        return;

    self->performSync(userdata);
}

static void SetPaintingDisabled(JNIEnv* env, jobject obj, int canvasSurTexMgr, int viewKey, jboolean disable)
{
    WebCore::CanvasSurfaceTextureManager* self =
                reinterpret_cast<WebCore::CanvasSurfaceTextureManager*>(canvasSurTexMgr);
    if (!self)
        return;

    self->setPaintingDisabled(viewKey, disable);
}

/*
 * JNI registration
 */
static JNINativeMethod g_canvasManagerMethods[] = {
    { "nativeSendSurfaceTexture", "(Landroid/graphics/SurfaceTexture;III)Z",
        (void*) SendSurfaceTexture },
    { "nativePerformSync", "(II)V", (void*) PerformSync },
    { "nativeSetPaintingDisabled", "(IIZ)V", (void*) SetPaintingDisabled },
};

int registerCanvasSurfaceTextureManager(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, g_canvasManagerJavaClass,
            g_canvasManagerMethods, NELEM(g_canvasManagerMethods));
}

} // namespace android

#endif // #if USE(ACCELERATED_COMPOSITING) && ENABLE(ACCELERATED_2D_CANVAS_ANDROID)

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
#define ONE_SECOND (1000000)
#define SAMPLE_PERIOD (5 * ONE_SECOND)
#define PRODUCER "Producer"
#define CONSUMER "Consumer"

namespace WebCore {

PassRefPtr<CanvasFpsCounter> CanvasFpsCounter::create(bool isProducer, int id)
{
    RefPtr<CanvasFpsCounter> fpsCounter = adoptRef(new CanvasFpsCounter(isProducer, id));
    return fpsCounter.release();
}

CanvasFpsCounter::CanvasFpsCounter(bool isProducer, int id) : m_isProducer(isProducer), m_id(id)
{
    reset();
}

void CanvasFpsCounter::frameProcessed()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t t = tv.tv_usec + (tv.tv_sec * ONE_SECOND);

    if (t < m_startTime) {
        // Overflow. Start from scratch.
        reset();
        return;
    }

    m_totalFrames++;
    uint64_t diff = t - m_lastSampleTime;
    if (diff >= SAMPLE_PERIOD) {
        uint64_t totalDiff = t - m_startTime;
        uint32_t averageFPS = (m_totalFrames * ONE_SECOND) / totalDiff;
        uint32_t instantFPS = ((m_totalFrames - m_lastSampledFrames) * ONE_SECOND) / diff;

        ALOGI("%s(%d) FPS - Instantaneous: %u Average: %u",
                      (m_isProducer ? PRODUCER : CONSUMER), m_id, instantFPS, averageFPS);

        m_lastSampleTime    = t;
        m_lastSampledFrames = m_totalFrames;
    }
}

void CanvasFpsCounter::reset()
{
    ALOGI("Resetting %s(%d) FPS counter", (m_isProducer ? PRODUCER : CONSUMER), m_id);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    m_startTime = tv.tv_usec + (tv.tv_sec * ONE_SECOND);
    m_lastSampleTime = m_startTime;
    m_totalFrames = 0;
    m_lastSampledFrames = 0;
}
} // namespace WebCore
#endif //ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)

