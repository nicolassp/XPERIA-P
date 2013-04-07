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

#ifndef Canvas2DLayerAndroid_h
#define Canvas2DLayerAndroid_h

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID) && USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "LayerAndroid.h"
#include <utils/threads.h>
#include <wtf/RefCounted.h>
#include <wtf/PassOwnPtr.h>
#include <GLES2/gl2.h>



namespace WebCore {

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
class CanvasFpsCounter;
#endif

class Canvas2DLayerAndroid;

class Canvas2DLayerAndroid : public LayerAndroid, public RefCounted<Canvas2DLayerAndroid> {

public:
    static PassOwnPtr<Canvas2DLayerAndroid> create();
    Canvas2DLayerAndroid();
    explicit Canvas2DLayerAndroid(const Canvas2DLayerAndroid& layer);
    virtual ~Canvas2DLayerAndroid();

    virtual bool drawGL();

    virtual LayerAndroid* copy() const { return new Canvas2DLayerAndroid(*this); }

    virtual bool isCanvas() const { return true; }

    void setTexture(GLuint textureId);

    void setContentBoxRect(IntRect& contentRect);

private:
    bool                        m_isAlpha;
    android::Mutex              m_textureLock;
    /* Variable below should be protected by m_textureLock */
    GLuint                      m_textureId;
    IntRect                     m_contentRect;

#if ENABLE(ACCELERATED_2D_CANVAS_ANDROID_FPS_COUNTER)
    RefPtr<CanvasFpsCounter>    m_fpsCounter;
#endif
};

} // namespace WebCore

#endif // #if ENABLE(ACCELERATED_2D_CANVAS_ANDROID) && USE(ACCELERATED_COMPOSITING)

#endif
