/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) ST-Ericsson SA 2012
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG ("Canvas2DLayerAndroid")
#include "AndroidLogging.h"

#include "config.h"
#include "Canvas2DLayerAndroid.h"

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID) && USE(ACCELERATED_COMPOSITING)

#include "PaintedSurface.h"
#include "TilesManager.h"
#include "TransformationMatrix.h"
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>

namespace WebCore {

PassOwnPtr<Canvas2DLayerAndroid> Canvas2DLayerAndroid::create()
{
    return adoptPtr(new Canvas2DLayerAndroid());
}

Canvas2DLayerAndroid::Canvas2DLayerAndroid() : LayerAndroid((RenderLayer*) NULL)
                                            , m_isAlpha(true)
                                            , m_textureId(0)
                                            , m_contentRect(IntRect())
#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
                                            , m_fpsCounter(CanvasFpsCounter::create(false, uniqueId()))
#endif
{
    ALOGD("%s: Thread: %d, this: %p, id: %d", __func__, pthread_self(), this, uniqueId());
}

Canvas2DLayerAndroid::Canvas2DLayerAndroid(const Canvas2DLayerAndroid& layer) : LayerAndroid(layer)
                        , RefCounted<Canvas2DLayerAndroid>()
                        , m_isAlpha(layer.m_isAlpha)
                        , m_textureId(layer.m_textureId)
                        , m_contentRect(layer.m_contentRect.location(), layer.m_contentRect.size())
#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
                        , m_fpsCounter(layer.m_fpsCounter)
#endif
{
    ALOGD("%s: Thread: %d, this: %p, id: %d", __func__, pthread_self(), this, uniqueId());
}

Canvas2DLayerAndroid::~Canvas2DLayerAndroid()
{
    ALOGD("%s: Thread: %d, this: %p, id: %d", __func__, pthread_self(), this, uniqueId());
}

void Canvas2DLayerAndroid::setContentBoxRect(IntRect& contentRect)
{
    m_contentRect.setLocation(contentRect.location());
    m_contentRect.setSize(contentRect.size());
}

void Canvas2DLayerAndroid::setTexture(GLuint textureId)
{
    android::Mutex::Autolock lock(m_textureLock);
    m_textureId = textureId;
}

bool Canvas2DLayerAndroid::drawGL()
{

    FloatRect clippingRect = TilesManager::instance()->shader()->rectInScreenCoord(drawClip());
    TilesManager::instance()->shader()->clip(clippingRect);
    if (!m_visible)
        return false;

    ALOGD("%s: Thread: %d, this: %p, id: %d", __func__, pthread_self(), this, uniqueId());

    GLuint textureId;
    { // Scope for lock
      android::Mutex::Autolock lock(m_textureLock);
      textureId = m_textureId;
    }

    // Paint the background (if any)
    PaintedSurface* backgroundTexture = texture();
    if (picture() && backgroundTexture) {
        ALOGV("Canvas2DLayerAndroid::drawGL(): painting background");
        backgroundTexture->draw();
    }

    // Paint the foreground (if needed)
    if (textureId > 0) {
        ALOGV("Canvas2DLayerAndroid::drawGL(): painting foreground");

        SkRect canvasBounds;
        if (m_contentRect.isEmpty()) {
            canvasBounds.setXYWH(0, 0, getWidth(), getHeight());
        } else {
            canvasBounds.setXYWH(m_contentRect.x(), m_contentRect.y(),
                                 m_contentRect.width(), m_contentRect.height());
        }


        TilesManager::instance()->shader()->drawLayerQuad(m_drawTransform, canvasBounds,
                                                          textureId,
                                                          1.0f, m_isAlpha,
                                                          GL_TEXTURE_EXTERNAL_OES);
    } else if (textureId <= 0) {
        ALOGD("Canvas2DLayerAndroid::drawGL(): No texture received yet");
    }

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
    m_fpsCounter->frameProcessed();
#endif

    return drawChildrenGL();

}


} // namespace WebCore
#endif // #if ENABLE(ACCELERATED_2D_CANVAS_ANDROID) && USE(ACCELERATED_COMPOSITING)
