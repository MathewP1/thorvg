/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _TVG_PICTURE_IMPL_H_
#define _TVG_PICTURE_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Picture::Impl
{
    unique_ptr<Loader> loader = nullptr;
    Paint* paint = nullptr;

    bool dispose(RenderMethod& renderer)
    {
        if (!paint) return false;

        paint->IMPL->dispose(renderer);
        delete(paint);

        return true;
    }

    bool update(RenderMethod &renderer, const RenderTransform* transform, RenderUpdateFlag flag)
    {
        if (loader) {
            auto scene = loader->data();
            if (scene) {
                this->paint = scene.release();
                if (!this->paint) return false;
                loader->close();
            }
        }

        if (!paint) return false;

        return paint->IMPL->update(renderer, transform, flag);
    }

    bool render(RenderMethod &renderer)
    {
        if (!paint) return false;
        return paint->IMPL->render(renderer);
    }

    bool viewbox(float* x, float* y, float* w, float* h)
    {
        if (!loader) return false;
        if (x) *x = loader->vx;
        if (y) *y = loader->vy;
        if (w) *w = loader->vw;
        if (h) *h = loader->vh;
        return true;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!paint) return false;
        return paint->IMPL->bounds(x, y, w, h);
    }

    Result load(const string& path)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader();
        if (!loader || !loader->open(path.c_str())) {
            //LOG: Non supported format
            return Result::NonSupport;
        }
        if (!loader->read()) return Result::Unknown;
        return Result::Success;
    }

    Result load(const char* data, uint32_t size)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader();
        if (!loader || !loader->open(data, size)) {
            //LOG: Non supported load data
            return Result::NonSupport;
        }
        if (!loader->read()) return Result::Unknown;
        return Result::Success;
    }
};

#endif //_TVG_PICTURE_IMPL_H_