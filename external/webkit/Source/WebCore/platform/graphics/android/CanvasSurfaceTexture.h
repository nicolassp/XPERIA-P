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
#ifndef CanvasSurfaceTexture_h
#define CanvasSurfaceTexture_h

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID)

#include "IntRect.h"
#include "GraphicsContext.h"
#include "PlatformGraphicsContext.h"

#include <SkGpuDevice.h>
#include <wtf/RefPtr.h>
#include <utils/RefBase.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#if USE(ACCELERATED_COMPOSITING)
#include "Canvas2DLayerAndroid.h"
#endif

using android::sp;

namespace android {
class SurfaceTexture;
}

namespace WebCore {
class HTMLCanvasElement;
class FrameView;
class CanvasSurfaceTextureManager;
class CanvasSurfaceTexture;

#if USE(ACCELERATED_COMPOSITING)
class LayerAndroid;
typedef LayerAndroid PlatformLayer;
#endif

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
class CanvasFpsCounter;
#endif
}

class SkCanvas;
class GrContext;
class GrRenderTarget;
class TextFlags;
class ANativeWindow;

class SkGpuDeviceWrapperFactory : public SkDeviceFactory {
public:
    SkGpuDeviceWrapperFactory(GrContext*, GrRenderTarget* rootRenderTarget);

    virtual ~SkGpuDeviceWrapperFactory();

    void setCanvasSurfaceTexture(PassRefPtr<WebCore::CanvasSurfaceTexture> cst)
    {    m_canvasSurfaceTexture = cst;    }

    virtual SkDevice* newDevice(SkCanvas* canvas, SkBitmap::Config config, int width,
                                int height, bool isOpaque, bool isLayer);

private:
    RefPtr<WebCore::CanvasSurfaceTexture> m_canvasSurfaceTexture;
    GrContext* m_context;
    GrRenderTarget* m_rootRenderTarget;
};

class SkGpuDeviceWrapper : public SkGpuDevice {
public:
    SkGpuDeviceWrapper(GrContext* context, SkDevice* bitmapDevice,
                       GrRenderTarget* renderTargetOrNull,
                       PassRefPtr<WebCore::CanvasSurfaceTexture> cst);

    virtual ~SkGpuDeviceWrapper();

    // overrides from SkDevice
    virtual void clear(SkColor color);
    virtual bool readPixels(const SkIRect& srcRect, SkBitmap* bitmap);
    virtual void writePixels(const SkBitmap& bitmap, int x, int y);

    virtual void setMatrixClip(const SkMatrix& matrix, const SkRegion& clip,
                               const SkClipStack&);

    virtual void drawPaint(const SkDraw&, const SkPaint& paint);
    virtual void drawPoints(const SkDraw&, SkCanvas::PointMode mode, size_t count,
                            const SkPoint points[], const SkPaint& paint);
    virtual void drawRect(const SkDraw&, const SkRect& r,
                          const SkPaint& paint);
    virtual void drawPath(const SkDraw&, const SkPath& path,
                          const SkPaint& paint, const SkMatrix* prePathMatrix,
                          bool pathIsMutable);
    virtual void drawBitmap(const SkDraw&, const SkBitmap& bitmap,
                            const SkIRect* srcRectOrNull,
                            const SkMatrix& matrix, const SkPaint& paint);
    virtual void drawSprite(const SkDraw&, const SkBitmap& bitmap,
                            int x, int y, const SkPaint& paint);
    virtual void drawText(const SkDraw&, const void* text, size_t len,
                          SkScalar x, SkScalar y, const SkPaint& paint);
    virtual void drawPosText(const SkDraw&, const void* text, size_t len,
                             const SkScalar pos[], SkScalar constY,
                             int scalarsPerPos, const SkPaint& paint);
    virtual void drawTextOnPath(const SkDraw&, const void* text, size_t len,
                                const SkPath& path, const SkMatrix* matrix,
                                const SkPaint& paint);
    virtual void drawVertices(const SkDraw&, SkCanvas::VertexMode, int vertexCount,
                              const SkPoint verts[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint);
    virtual void drawDevice(const SkDraw&, SkDevice*, int x, int y,
                            const SkPaint&);
    virtual bool filterTextFlags(const SkPaint& paint, TextFlags*);

    virtual void flush();


    /* request a makeCurrent with the underlying SurfaceTexture */
    virtual void makeRenderTargetCurrent();

    const SkBitmap& accessSwBitmap(bool changePixels) { return m_bitmapDevice->accessBitmap(changePixels); }

    SkDevice* getSoftwareDevice() { return m_bitmapDevice; }

    void resetDrawCount() { m_drawCount = 0; };

protected:
    virtual SkDeviceFactory* onNewDeviceFactory();
    virtual void onAccessBitmap(SkBitmap*);

private:
    RefPtr<WebCore::CanvasSurfaceTexture> m_canvasSurfaceTexture;
    GrContext*                            m_context;
    GrRenderTarget*                       m_renderTarget;
    SkDevice*                             m_bitmapDevice;
    int                                   m_drawCount;
};


namespace WebCore {

class CanvasSurfaceTexture : public RefCounted<CanvasSurfaceTexture> {
private:
    CanvasSurfaceTexture();

public:
    static PassRefPtr<CanvasSurfaceTexture> create() { return adoptRef(new CanvasSurfaceTexture());}
    virtual ~CanvasSurfaceTexture();
    bool reset(HTMLCanvasElement*);
    void clear();
    void markDirtyRect(const IntRect& rect);

    FrameView* frameView();

    void setSurfaceTexture(sp<android::SurfaceTexture> texture, GLuint textureId);
    void performSync();

    void setParentContext(GraphicsContext* ctx);

    void ensureCurrent();

    void flush(bool flushGL);

    void disablePainting(bool disable);

    bool isAccelerated() const { return m_isInitialized; }

    void readbackToSoftware();

#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer() const { return m_canvasLayer.get(); }
#endif

private:

    void registerCanvas();
    void deregisterCanvas();
    bool initEGL();
    bool setupWindowSurface(sp<android::SurfaceTexture> texture);
    bool setupCanvasAndRenderTarget(SkCanvas* skCanvas);

    int                           m_width;
    int                           m_height;
    bool                          m_isRegistered;
    GraphicsContext*              m_parentContext;
    HTMLCanvasElement*            m_canvasElement;

#if USE(ACCELERATED_COMPOSITING)
    OwnPtr<Canvas2DLayerAndroid>  m_canvasLayer;
#endif

    EGLContext                    m_eglContext;
    EGLDisplay                    m_eglDisplay;
    EGLSurface                    m_eglSurface;
    EGLConfig                     m_surfaceConfig;
    sp<ANativeWindow>             m_anw;
    GrContext*                    m_grContext;
    int                           m_id;
    bool                          m_waitingForSync;
    bool                          m_paintingDisabled;
    bool                          m_isInitialized;
    CanvasSurfaceTextureManager*  m_ctm;
    SkGpuDeviceWrapper*           m_device;

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
    RefPtr<CanvasFpsCounter>      m_fpsCounter;
#endif

};

} // namespace WebCore
#endif // ENABLE(ACCELERATED_2D_CANVAS_ANDROID)
#endif // CanvasSurfaceTexture_h
