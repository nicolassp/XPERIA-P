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
#ifndef CanvasSurfaceTextureManager_h
#define CanvasSurfaceTextureManager_h

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID)

#include <wtf/HashMap.h>
#include <utils/threads.h>
#include <utils/RefBase.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>


using android::sp;

namespace android {
    class SurfaceTexture;
}

namespace WebCore {

class CanvasSurfaceTexture;

struct CanvasSurfaceTextureInfo;
struct CanvasManagerInfo;

class CanvasSurfaceTextureManager {

public:
    typedef HashMap<int, CanvasSurfaceTextureInfo*>::const_iterator ContextIterator;
    typedef HashMap<int, CanvasManagerInfo*>::const_iterator ManagerIterator;

    CanvasSurfaceTextureManager();
    virtual ~CanvasSurfaceTextureManager();

    /**
     * Called when the platform graphics context is created.  A new SurfaceTexture
     * for this context will be requested from the Java side.
     * @param id - The unique ID of the platform layer belonging to this context.
     */
    void registerCanvasSurfaceTexture(int id, CanvasSurfaceTexture*);

    /**
     * Called when the platform graphics context is destroyed. The SurfaceTexture
     * associated with this context will be destroyed on the Java side.
     */
    void deregisterCanvasSurfaceTexture(int id);


    void requestSync(int id);

    bool setSurfaceTextureForContext(int id, sp<android::SurfaceTexture> texture, GLuint textureId);
    void performSync(int key);
    void setPaintingDisabled(int viewKey, bool disable);
    void ensureCurrent(int id, EGLDisplay display, EGLSurface surface, EGLContext context);

    struct JavaGlue;


private:

    void waitForSurfaceTexture(CanvasSurfaceTextureInfo* info);

    HashMap<int, CanvasSurfaceTextureInfo*> m_canvasSurfaceTextureInfoMap;
    HashMap<int, CanvasManagerInfo*>        m_canvasManagerMap;
    android::Mutex                          m_mapLock;
    android::Condition                      m_mapCond;
    JavaGlue*                               m_javaGlue;
    int                                     m_current;

};

} // namespace WebCore

#endif        //  USE(ACCELERATED_COMPOSITING) && ENABLE(ACCELERATED_2D_CANVAS_ANDROID)

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CanvasFpsCounter : public RefCounted<CanvasFpsCounter> {
public:
    static PassRefPtr<CanvasFpsCounter> create(bool isProducer, int id);
    virtual ~CanvasFpsCounter() {};

    void frameProcessed();

private:
    CanvasFpsCounter(bool isProducer, int id);
    void reset();
    bool     m_isProducer;
    int      m_id;
    uint32_t m_totalFrames;
    uint32_t m_lastSampledFrames;
    uint64_t m_startTime;
    uint64_t m_lastSampleTime;
};

}
#endif // ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)


#endif        //  CanvasSurfaceTextureManager_h
