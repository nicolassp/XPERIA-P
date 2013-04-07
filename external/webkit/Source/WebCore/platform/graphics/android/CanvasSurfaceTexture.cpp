 /*
 * Copyright 2006, The Android Open Source Project
 * Copyright (C) ST-Ericsson SA 2012
 * Copyright (C) 2012 Sony Mobile Communications AB.
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

#define LOG_TAG ("CanvasSurfaceTexture")
#include "AndroidLogging.h"

#include "config.h"

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID)

#include "CanvasSurfaceTexture.h"
#include "PlatformGraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "FrameView.h"
#include "Frame.h"
#include "android_graphics.h"


#include "TilesManager.h"
#include "Canvas2DLayerAndroid.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include <OwnArrayPtr.h>
#include <GrContext.h>
#include <SkCanvas.h>
#include <SkGpuDevice.h>
#include <SkGpuDeviceFactory.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <GLES2/gl2.h>


// Max number of textures held in the bitmap texture cache
static const int maxTextureCacheCount = 128;
// Max bytes allocated in the bitmap texture cache
static const size_t maxTextureCacheBytes = 24 * 1024 * 1024;
// Force a flush if the number of draw calls exceeds the threshold
static const int maxDrawCount = 1000;

#define WRAP_GPU_DEVICE_VOID_CALL(wrapped_func, args) \
    ALOGV("SkGpuDeviceWrapper - %s", __func__);  \
    makeRenderTargetCurrent();                   \
    wrapped_func args

#define WRAP_GPU_DEVICE_CALL(wrapped_func, args) \
    ALOGV("SkGpuDeviceWrapper - %s", __func__);  \
    makeRenderTargetCurrent();                   \
    return wrapped_func args

#define WRAP_GPU_DEVICE_COUNTING_VOID_CALL(wrapped_func, args) \
    ALOGV("SkGpuDeviceWrapper - %s", __func__);  \
    makeRenderTargetCurrent();                   \
    wrapped_func args;                            \
    if(++m_drawCount >= maxDrawCount) {          \
        m_canvasSurfaceTexture->performSync();   \
    }


//////////////////////// SkGpuDeviceWrapperFactory /////////////////////////////////

SkGpuDeviceWrapperFactory::SkGpuDeviceWrapperFactory(GrContext* context,
                                                     GrRenderTarget* rootRenderTarget)
                : m_canvasSurfaceTexture()
{
    GrAssert(NULL != context);
    GrAssert(NULL != rootRenderTarget);
    m_rootRenderTarget = rootRenderTarget;
    rootRenderTarget->ref();
    m_context = context;
    context->ref();
}

SkGpuDeviceWrapperFactory::~SkGpuDeviceWrapperFactory()
{
    m_rootRenderTarget->unref();
    m_context->unref();
}

SkDevice* SkGpuDeviceWrapperFactory::newDevice(SkCanvas* canvas, SkBitmap::Config config, int width,
                                                int height, bool , bool isLayer)
{
    GrAssert(m_canvasSurfaceTexture);
    GrAssert(canvas);
    GrAssert(canvas->getDevice());

    return new SkGpuDeviceWrapper(m_context, canvas->getDevice(),
                                  isLayer ?  NULL : m_rootRenderTarget,
                                  m_canvasSurfaceTexture);
}

//////////////////////// SkGpuDeviceWrapper /////////////////////////////////

SkGpuDeviceWrapper::SkGpuDeviceWrapper(GrContext* context, SkDevice* bitmapDevice,
                                       GrRenderTarget* renderTargetOrNull,
                                       PassRefPtr<WebCore::CanvasSurfaceTexture> cst)
              : SkGpuDevice(context, bitmapDevice->accessBitmap(false), renderTargetOrNull)
              , m_canvasSurfaceTexture(cst)
              , m_context(context)
              , m_renderTarget(renderTargetOrNull)
              , m_bitmapDevice(bitmapDevice)
              , m_drawCount(0)
{
    bitmapDevice->ref();
    bitmapDevice->lockPixels();
}

SkGpuDeviceWrapper::~SkGpuDeviceWrapper()
{
    m_bitmapDevice->unlockPixels();
    m_bitmapDevice->unref();
}

void SkGpuDeviceWrapper::makeRenderTargetCurrent()
{
    m_canvasSurfaceTexture->ensureCurrent();
}

SkDeviceFactory* SkGpuDeviceWrapper::onNewDeviceFactory()
{
    return SkNEW_ARGS(SkGpuDeviceWrapperFactory, (m_context, m_renderTarget));
}

void SkGpuDeviceWrapper::onAccessBitmap(SkBitmap* bm)
{
    ALOGV("SkGpuDeviceWrapper - onAccessBitmap() %p", bm);
}

void SkGpuDeviceWrapper::clear(SkColor color)
{
    WRAP_GPU_DEVICE_VOID_CALL(SkGpuDevice::clear, (color));
}

bool SkGpuDeviceWrapper::readPixels(const SkIRect& srcRect, SkBitmap* bitmap)
{
    WRAP_GPU_DEVICE_CALL(SkGpuDevice::readPixels, (srcRect, bitmap));
}

void SkGpuDeviceWrapper::writePixels(const SkBitmap& bitmap, int x, int y)
{
    WRAP_GPU_DEVICE_VOID_CALL(SkGpuDevice::writePixels, (bitmap, x, y));
}

void SkGpuDeviceWrapper::setMatrixClip(const SkMatrix& matrix,
                                       const SkRegion& clip,
                                       const SkClipStack& clipStack)
{
    WRAP_GPU_DEVICE_VOID_CALL(SkGpuDevice::setMatrixClip, (matrix, clip, clipStack));
}

void SkGpuDeviceWrapper::drawPaint(const SkDraw& draw, const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawPaint, (draw, paint));
}

void SkGpuDeviceWrapper::drawPoints(const SkDraw& draw, SkCanvas::PointMode mode,
                                    size_t count, const SkPoint points[], const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawPoints, (draw, mode, count, points, paint));
}

void SkGpuDeviceWrapper::drawRect(const SkDraw& draw, const SkRect& r,
                                  const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawRect, (draw, r, paint));
}

void SkGpuDeviceWrapper::drawPath(const SkDraw& draw, const SkPath& path,
                                  const SkPaint& paint, const SkMatrix* prePathMatrix,
                                  bool pathIsMutable)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawPath, (draw, path, paint, prePathMatrix, pathIsMutable));
}

void SkGpuDeviceWrapper::drawBitmap(const SkDraw& draw, const SkBitmap& bitmap,
                                    const SkIRect* srcRectOrNull,
                                    const SkMatrix& matrix, const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawBitmap, (draw, bitmap, srcRectOrNull, matrix, paint));
}

void SkGpuDeviceWrapper::drawSprite(const SkDraw& draw, const SkBitmap& bitmap,
                                    int x, int y, const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawSprite, (draw, bitmap, x, y, paint));
}

void SkGpuDeviceWrapper::drawText(const SkDraw& draw, const void* text, size_t len,
                                  SkScalar x, SkScalar y, const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawText, (draw, text, len, x, y, paint));
}

void SkGpuDeviceWrapper::drawPosText(const SkDraw& draw, const void* text, size_t len,
                                     const SkScalar pos[], SkScalar constY,
                                     int scalarsPerPos, const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawPosText, (draw, text, len, pos, constY, scalarsPerPos, paint));
}

void SkGpuDeviceWrapper::drawTextOnPath(const SkDraw& draw, const void* text, size_t len,
                                        const SkPath& path, const SkMatrix* matrix,
                                        const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawTextOnPath, (draw, text, len, path, matrix, paint));
}

void SkGpuDeviceWrapper::drawVertices(const SkDraw& draw, SkCanvas::VertexMode mode, int vertexCount,
                                      const SkPoint verts[], const SkPoint texs[],
                                      const SkColor colors[], SkXfermode* xmode,
                                      const uint16_t indices[], int indexCount,
                                      const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawVertices, (draw, mode, vertexCount, verts, texs,
                         colors, xmode, indices, indexCount, paint));
}

void SkGpuDeviceWrapper::drawDevice(const SkDraw& draw, SkDevice* dev, int x, int y,
                                    const SkPaint& paint)
{
    WRAP_GPU_DEVICE_COUNTING_VOID_CALL(SkGpuDevice::drawDevice, (draw, dev, x, y, paint));
}

bool SkGpuDeviceWrapper::filterTextFlags(const SkPaint& paint, TextFlags* flags)
{
    WRAP_GPU_DEVICE_CALL(SkGpuDevice::filterTextFlags, (paint, flags));
}

void SkGpuDeviceWrapper::flush()
{
    WRAP_GPU_DEVICE_VOID_CALL(SkGpuDevice::flush, ());
}

//////////////////////// CanvasSurfaceTexture /////////////////////////////////

namespace WebCore {

#if !USE(ACCELERATED_COMPOSITING)
// If we don't have a layer, we need to generate an Id ourselves
static int gUniqueId = 0;
#endif

CanvasSurfaceTexture::CanvasSurfaceTexture()
        : m_width(-1)
        , m_height(-1)
        , m_isRegistered(false)
        , m_parentContext(0)
        , m_canvasElement()
#if USE(ACCELERATED_COMPOSITING)
        , m_canvasLayer(Canvas2DLayerAndroid::create())
#endif
        , m_eglContext(EGL_NO_CONTEXT)
        , m_eglDisplay(EGL_NO_DISPLAY)
        , m_eglSurface(EGL_NO_SURFACE)
        , m_surfaceConfig(0)
        , m_anw(0)
        , m_grContext(0)
#if USE(ACCELERATED_COMPOSITING)
        , m_id(m_canvasLayer->uniqueId())
#else
        , m_id(++gUniqueId)
#endif
        , m_waitingForSync(false)
        , m_paintingDisabled(false)
        , m_isInitialized(false)
        , m_ctm(TilesManager::instance()->canvasSurfaceTextureManager())
        , m_device(0)
#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
        , m_fpsCounter(CanvasFpsCounter::create(true, m_id))
#endif

{
    ALOGD("Constructor called this: %p id: %d", this, m_id);

    if(!initEGL()) {
        ALOGD("initEGL failed");
    }

}

CanvasSurfaceTexture::~CanvasSurfaceTexture()
{
    ALOGD("Destructor called this: %p id: %d thread: %d", this, m_id, pthread_self());

    clear();
}

FrameView* CanvasSurfaceTexture::frameView()
{
    if (m_canvasElement && m_canvasElement->document()->frame())
        return m_canvasElement->document()->frame()->view();

    return 0;
}

/* Called on the WebCore Thread */
void CanvasSurfaceTexture::setSurfaceTexture(sp<android::SurfaceTexture> texture,
                                             GLuint textureId)
{
    if (!texture.get() || textureId == 0) {
        ALOGW("%s: Invalid arguments!", __func__);
        return;
    }

    static const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    if(!setupWindowSurface(texture)) {
        ALOGD("setupWindowSurface failed");
        return;
    }

#if USE(ACCELERATED_COMPOSITING)
    /* Notify the layer that we have a texture */
    m_canvasLayer->setTexture(textureId);
#endif

}

void CanvasSurfaceTexture::performSync()
{
    if (m_grContext && m_waitingForSync) {
        ensureCurrent();

        ALOGD("performSync() this=%p, id=%d", this, m_id);
        flush(false);

        ALOGD("performSync() after grFlush!");
        eglSwapBuffers(m_eglDisplay, m_eglSurface);
        GLUtils::checkEglError("eglSwapBuffers");
        ALOGD("performSync() after eglSwapBuffers");
        m_waitingForSync = false;

        m_device->resetDrawCount();

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
        m_fpsCounter->frameProcessed();
#endif
    }
}

bool CanvasSurfaceTexture::reset(HTMLCanvasElement* canvasElement)
{
    if(!canvasElement) {
        ALOGD("no canvasElement....");
        return false;
    }

    /* Our current parent context is no longer valid.  We will soon get a new one */
    m_parentContext = 0;


    /* If we have the same frameView we can re-use the SurfaceTexture,
       If not we have to cleanup and start from scratch */
    FrameView* prevFrameView = 0;
    FrameView* currFrameView = 0;

    if (m_canvasElement && m_canvasElement->document()->frame())
        prevFrameView = m_canvasElement->document()->frame()->view();

    if (canvasElement->document()->frame())
       currFrameView = canvasElement->document()->frame()->view();

    if (prevFrameView && (prevFrameView != currFrameView)) {
        ALOGD("reset() frameView changed!");
        clear();
        return reset(canvasElement);
    }

    m_canvasElement = canvasElement;
    IntSize newSize(canvasElement->width(), canvasElement->height());

    if((newSize.width() == m_width && newSize.height() == m_height)) {
        ALOGD("reset() same size");
        return true;
    }

    ensureCurrent();
    if (m_grContext) {
        m_grContext->unref();
        m_grContext = 0;
    }

    m_width = newSize.width();
    m_height = newSize.height();

    if (!m_isRegistered) {
        registerCanvas();
        /* registerCanvas implicitly calls setupWindowSurface
         * if successful. So we should have an m_eglSurface by now */
        return m_eglSurface != EGL_NO_SURFACE;

    } else if(!setupWindowSurface(NULL)) {
        return false;
    }

    /* Now all that's left is to create and attach a GpuDevice to the SkCanvas
     * in our parent context's PlatformGraphicsContext.
     * but first we need to wait for our new parentContext.
     */
    return true;
}

void CanvasSurfaceTexture::setParentContext(GraphicsContext* ctx)
{
    m_parentContext = ctx;


    if(!setupCanvasAndRenderTarget(ctx->platformContext()->mCanvas)) {
        ALOGE("setParentContext(): setupCanvasAndRenderTarget failed!");
        ctx->setCanvasSurfaceTexture(0);
        m_parentContext = 0;
        clear();
        return;
    }

    m_grContext->resetContext();

    /* Start with an empty frame */
    ctx->clearRect(FloatRect(0, 0, m_width, m_height));
    eglSwapBuffers(m_eglDisplay, m_eglSurface);

    m_parentContext->setPaintingDisabled(false);
    m_isInitialized = true;
}

void CanvasSurfaceTexture::clear()
{
    if (m_eglContext && m_eglSurface)
        ensureCurrent();

    if (m_grContext)
        m_grContext->unref();

    m_anw = 0;

    if (m_eglSurface) {
        eglDestroySurface(m_eglDisplay, m_eglSurface);
        GLUtils::checkEglError("eglDestroySurface");
        m_eglSurface = EGL_NO_SURFACE;
    }

    if (m_eglContext) {
        eglDestroyContext(m_eglDisplay, m_eglContext);
        GLUtils::checkEglError("eglDestroyContext");
        m_eglContext = EGL_NO_CONTEXT;
    }

#if USE(ACCELERATED_COMPOSITING)
    m_canvasLayer->setTexture(-1);
#endif

    /* release the current surface */
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    deregisterCanvas();

    m_isRegistered = false;
    m_isInitialized = false;
    m_width = -1;
    m_height = -1;
}

void CanvasSurfaceTexture::markDirtyRect(const IntRect& rect)
{
    if (!m_waitingForSync && m_isRegistered && !m_paintingDisabled) {
        ALOGD("markDirtyRect() Requesting sync! id=%d", m_id);
        m_ctm->requestSync(m_id);
        m_waitingForSync = true;
    } else if (!m_isRegistered) {
        ALOGD("markDirtyRect() unable to request sync!");
    }
}

void CanvasSurfaceTexture::registerCanvas()
{
    if (!m_isRegistered) {
        ALOGD("registerCanvas() called id: %d", m_id);

        /* Register ourselves with the CanvasTextureManager */
        m_ctm->registerCanvasSurfaceTexture(m_id, this);
        m_isRegistered = true;
    }
}

void CanvasSurfaceTexture::deregisterCanvas()
{
    if (m_isRegistered) {
        ALOGD("deregisterCanvas() called id: %d", m_id);

        m_ctm->deregisterCanvasSurfaceTexture(m_id);
        m_isRegistered = false;
    }

}

void CanvasSurfaceTexture::ensureCurrent()
{
    if (m_eglSurface && m_eglContext && m_eglDisplay) {
        m_ctm->ensureCurrent(m_id, m_eglDisplay, m_eglSurface, m_eglContext);
    }
}

void CanvasSurfaceTexture::flush(bool flushGL)
{
    if (m_grContext) {
        m_grContext->flush();

        if (flushGL)
            glFlush();
    }
}

void CanvasSurfaceTexture::readbackToSoftware()
{
    if (!m_isInitialized)
        return;

    const SkBitmap& bm = m_device->accessSwBitmap(true);
    SkAutoLockPixels alp(bm);
    if (!bm.getPixels()) {
        ALOGW("readbackToSoftware() ...... no pixels allocated in destination bitmap!");
        return;
    }

    m_device->readPixels(SkIRect::MakeWH(bm.width(), bm.height()), (SkBitmap *)&bm);

    m_waitingForSync = true;
    performSync();
}

void CanvasSurfaceTexture::disablePainting(bool disable)
{
    if (m_parentContext) {
        ALOGD("%s(): disabling painting: %d: ", __func__, disable);
        m_paintingDisabled = disable;
        m_parentContext->setPaintingDisabled(disable);
    } else {
        ALOGW("%s(): unable to notify parent!", __func__);
    }
}

bool CanvasSurfaceTexture::initEGL()
{
    EGLint numConfigs;
    static const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    static const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLBoolean returnValue;
    EGLint majorVersion;
    EGLint minorVersion;

    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        GLUtils::checkEglError("eglGetDisplay");
        return false;
    }

    if (!eglInitialize(m_eglDisplay, &majorVersion, &minorVersion))
    {
        GLUtils::checkEglError("eglInitialize");
        return false;
    }

    if (eglChooseConfig(m_eglDisplay, configAttribs, &m_surfaceConfig, 1, &numConfigs) != EGL_TRUE) {
        ALOGE("initEGL(): eglChooseConfig failed!");
        return false;
    }

    m_eglContext = eglCreateContext(m_eglDisplay, m_surfaceConfig, EGL_NO_CONTEXT, contextAttribs);
    GLUtils::checkEglError("eglCreateContext");
    ALOGD("eglCreateContext: %p", m_eglContext);

    return m_eglContext != EGL_NO_CONTEXT;
}

bool CanvasSurfaceTexture::setupWindowSurface(sp<android::SurfaceTexture> texture)
{
    if(texture.get()) {
        m_anw = new android::SurfaceTextureClient(texture);

        texture->setBufferCount(5);
    }

    if (!m_anw.get()) {
        ALOGW("%s: m_anw = null. error", __func__);
        return false;
    }

    int result = ANativeWindow_setBuffersGeometry(m_anw.get(),
                                                  m_width, m_height, WINDOW_FORMAT_RGBA_8888);
    if (result < 0) {
        ALOGE("ANW setBufferGeometry failed for width: %d and height: %d. Result: %d",
                                                                      m_width, m_height, result);
        return false;
    }

    if (!texture.get() && m_eglSurface)
        return true; // No new texture -> re-use egl surface

    if (m_eglSurface) {
        ALOGD("Destroying previous eglSurface..");
        eglDestroySurface(m_eglDisplay, m_eglSurface);
        GLUtils::checkEglError("eglDestroySurface");
        m_eglSurface = EGL_NO_SURFACE;
    }

    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_surfaceConfig, m_anw.get(), NULL);
    GLUtils::checkEglError("eglCreateWindowSurface");
    ALOGD("eglCreateWindowSurface: %p", m_eglSurface);

    EGLBoolean returnValue = eglSurfaceAttrib(m_eglDisplay, m_eglSurface,
                                   EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
    ALOGD("eglSurfaceAttrib result: %d", returnValue);
    GLUtils::checkEglError("eglSurfaceAttrib", returnValue);

    return true;
}

bool CanvasSurfaceTexture::setupCanvasAndRenderTarget(SkCanvas* skCanvas)
{
    if (!skCanvas) {
        ALOGE("setupCanvasAndRenderTarget() canvas was NULL!");
        return false;
    }

    ensureCurrent();

    if (!m_grContext) {
        m_grContext = GrContext::Create(kOpenGL_Shaders_GrEngine, NULL);
        if (!m_grContext) {
            ALOGE("%s: m_grContext == NULL!", __func__);
            return false;
        }
        m_grContext->setTextureCacheLimits(maxTextureCacheCount, maxTextureCacheBytes);
        ALOGD("Created GrContext: %p", m_grContext);
    }

    GrPlatformSurfaceDesc desc;
    desc.fSurfaceType = kRenderTarget_GrPlatformSurfaceType;
    desc.fRenderTargetFlags = kNone_GrPlatformRenderTargetFlagBit;
    desc.fWidth = SkScalarRound(m_width);
    desc.fHeight = SkScalarRound(m_height);
    desc.fConfig = kRGBA_8888_GrPixelConfig;
    desc.fStencilBits = 8;
    desc.fPlatformRenderTarget = 0;

    GrRenderTarget* grTarget =
                        static_cast<GrRenderTarget*>(m_grContext->createPlatformSurface(desc));
    if (!grTarget) {
        ALOGD("%s: grTarget == NULL!", __func__);
        return false;
    }

    SkGpuDeviceWrapperFactory* factory = new SkGpuDeviceWrapperFactory(m_grContext, grTarget);
    factory->setCanvasSurfaceTexture(this);

    m_device = static_cast<SkGpuDeviceWrapper*>(factory->newDevice(skCanvas,
                           SkBitmap::kARGB_8888_Config,desc.fWidth, desc.fHeight, false, false));
    ALOGD("Created SkGpuDevice: %p", m_device);
    skCanvas->setDevice(m_device)->unref();
    skCanvas->setDeviceFactory(factory)->unref();

    grTarget->unref();
    return true;
}

}   // WebCore

#endif /* ENABLE(ACCELERATED_2D_CANVAS_ANDROID)*/
