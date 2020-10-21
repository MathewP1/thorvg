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
#ifndef _TVG_SHAPE_IMPL_H_
#define _TVG_SHAPE_IMPL_H_

#include <memory.h>
#include "tvgPaint.h"
#include "tvgBezier.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ShapePath
{
    PathCommand* cmds = nullptr;
    uint32_t cmdCnt = 0;
    uint32_t reservedCmdCnt = 0;

    Point *pts = nullptr;
    uint32_t ptsCnt = 0;
    uint32_t reservedPtsCnt = 0;

    ~ShapePath()
    {
        if (cmds) free(cmds);
        if (pts) free(pts);
    }

    ShapePath() {}

    ShapePath(const ShapePath* src)
    {
        cmdCnt = src->cmdCnt;
        reservedCmdCnt = src->reservedCmdCnt;
        ptsCnt = src->ptsCnt;
        reservedPtsCnt = src->reservedPtsCnt;

        cmds = static_cast<PathCommand*>(malloc(sizeof(PathCommand) * reservedCmdCnt));
        if (!cmds) return;
        memcpy(cmds, src->cmds, sizeof(PathCommand) * cmdCnt);

        pts = static_cast<Point*>(malloc(sizeof(Point) * reservedPtsCnt));
        if (!pts) {
            free(cmds);
            return;
        }
        memcpy(pts, src->pts, sizeof(Point) * ptsCnt);
    }

    void reserveCmd(uint32_t cmdCnt)
    {
        if (cmdCnt <= reservedCmdCnt) return;
        reservedCmdCnt = cmdCnt;
        cmds = static_cast<PathCommand*>(realloc(cmds, sizeof(PathCommand) * reservedCmdCnt));
    }

    void reservePts(uint32_t ptsCnt)
    {
        if (ptsCnt <= reservedPtsCnt) return;
        reservedPtsCnt = ptsCnt;
        pts = static_cast<Point*>(realloc(pts, sizeof(Point) * reservedPtsCnt));
    }

    void grow(uint32_t cmdCnt, uint32_t ptsCnt)
    {
        reserveCmd(this->cmdCnt + cmdCnt);
        reservePts(this->ptsCnt + ptsCnt);
    }

    void reset()
    {
        cmdCnt = 0;
        ptsCnt = 0;
    }

    void append(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt)
    {
        memcpy(this->cmds + this->cmdCnt, cmds, sizeof(PathCommand) * cmdCnt);
        memcpy(this->pts + this->ptsCnt, pts, sizeof(Point) * ptsCnt);
        this->cmdCnt += cmdCnt;
        this->ptsCnt += ptsCnt;
    }

    void moveTo(float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 2 > reservedPtsCnt) reservePts((ptsCnt + 2) * 2);

        cmds[cmdCnt++] = PathCommand::MoveTo;
        pts[ptsCnt++] = {x, y};
    }

    void lineTo(float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 2 > reservedPtsCnt) reservePts((ptsCnt + 2) * 2);

        cmds[cmdCnt++] = PathCommand::LineTo;
        pts[ptsCnt++] = {x, y};
    }

    void cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 3 > reservedPtsCnt) reservePts((ptsCnt + 3) * 2);

        cmds[cmdCnt++] = PathCommand::CubicTo;
        pts[ptsCnt++] = {cx1, cy1};
        pts[ptsCnt++] = {cx2, cy2};
        pts[ptsCnt++] = {x, y};
    }

    void close()
    {
        if (cmdCnt > 0 && cmds[cmdCnt - 1] == PathCommand::Close) return;

        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        cmds[cmdCnt++] = PathCommand::Close;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (ptsCnt == 0) return false;

        Point min = { pts[0].x, pts[0].y };
        Point max = { pts[0].x, pts[0].y };

        for(uint32_t i = 1; i < ptsCnt; ++i) {
            if (pts[i].x < min.x) min.x = pts[i].x;
            if (pts[i].y < min.y) min.y = pts[i].y;
            if (pts[i].x > max.x) max.x = pts[i].x;
            if (pts[i].y > max.y) max.y = pts[i].y;
        }

        if (x) *x = min.x;
        if (y) *y = min.y;
        if (w) *w = max.x - min.x;
        if (h) *h = max.y - min.y;

        return true;
    }
};

struct Stencil
{
    struct Intersection
    {
        Point p;
        float t;
        uint32_t inner, outer, inner_pts, outer_pts;
    };

    ShapePath* stencil = nullptr;
    ShapePath* t_stencil = nullptr;

    Stencil(ShapePath& first)
    {
        printf("Creating stencil...\n");
        stencil = new ShapePath();
        stencil->cmdCnt = first.cmdCnt;
        stencil->ptsCnt = first.ptsCnt;
        stencil->reserveCmd(first.cmdCnt);
        stencil->reservePts(first.ptsCnt);
        memcpy(stencil->cmds, first.cmds, sizeof(PathCommand) * first.cmdCnt);
        memcpy(stencil->pts, first.pts, sizeof(Point) * first.ptsCnt);
        t_stencil = new ShapePath();
    }

    ~Stencil()
    {
        if (stencil) delete(stencil);
        if (t_stencil) delete(t_stencil);
        if (all_intersections) free(all_intersections);
    }

    void Update(ShapePath& src)
    {
        printf("Updating stencil...\n");
        copyContour(src);

        if (!all_intersections) all_intersections = static_cast<Intersection*>(calloc(alloc_intersections, sizeof(Intersection)));

        Point outer_line[2], inner_line[2];
        Point outer_cubic[4], inner_cubic[4];
        PathCommand inner_cmd, outer_cmd;
        uint32_t i_outer = 0; uint32_t i_outer_pts = 0;
        uint32_t i_inner = 0; uint32_t i_inner_pts = 0;

        do{
            i_inner = i_inner_pts = 0;
            outer_cmd = stencil->cmds[i_outer];

            if (outer_cmd == PathCommand::MoveTo) continue;
            if (outer_cmd == PathCommand::Close) {
                getPoints(*stencil, i_outer, i_outer_pts, outer_line);
                if (outer_line[0].x == outer_line[1].x && outer_line[0].y == outer_line[1].y) continue;
            }

            if (outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo){
                getPoints(*stencil, i_outer, i_outer_pts, outer_line);
            }

            if (outer_cmd == PathCommand::CubicTo){
                getPoints(*stencil, i_outer, i_outer_pts, outer_cubic);
            }

            do{
                inner_cmd = t_stencil->cmds[i_inner];

                if (inner_cmd == PathCommand::MoveTo) continue;
                if (inner_cmd == PathCommand::Close){
                    getPoints(*t_stencil, i_inner, i_inner_pts, inner_line);
                    if (inner_line[0].x == inner_line[1].x && inner_line[0].y == inner_line[1].y) continue;
                }

                if (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo){
                    getPoints(*t_stencil, i_inner, i_inner_pts, inner_line);
                }

                if (inner_cmd == PathCommand::CubicTo){
                    getPoints(*t_stencil, i_inner, i_inner_pts, inner_cubic);
                }

                Intersection intersections[9]; //max intersection (cubic with cubic)
                uint32_t num_intersections = 0;

                if ((outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo)
                    && (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo)){
                    // get segment-segment intersection
                }

                if ((outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo)
                    && inner_cmd == PathCommand::CubicTo){
                    // get segment-cubic intersection
                }

                if ((outer_cmd == PathCommand::CubicTo)
                    && (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo)) {
                    // get cubic-segment intersections
                }

                if (outer_cmd == PathCommand::CubicTo && inner_cmd == PathCommand::CubicTo){
                    // get cubic-cubic intersecitons
                }

                if (num_intersections){
                    // add intersecions
                }

                if (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo) {
                    i_inner_pts++;
                }
                if (inner_cmd == PathCommand::CubicTo){
                    i_inner_pts+=3;
                }
            }while(++i_inner < t_stencil->cmdCnt);

            if (outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo){
                i_outer_pts++;
            }
            if (outer_cmd == PathCommand::CubicTo){
                i_outer_pts+=3;
            }
        }while(++i_outer < stencil->cmdCnt);

        // split paths at intersections
        // find outside points and build stencil path
    }

private:
    Intersection* all_intersections = nullptr;
    uint32_t num_all_intersections = 0;
    uint32_t alloc_intersections = 4;

    void copyContour(ShapePath& src)
    {
        // TODO: more robust impl, now it handles only predefined shapes (no weird paths)
        int i = src.cmdCnt - 2;
        int i_pts = src.ptsCnt;
        while (i - 1 >= 0){
            switch(src.cmds[i]){
                case PathCommand::MoveTo: {
                    i_pts--;
                    break;
                }
                case PathCommand::LineTo: {
                    i_pts--;
                    break;
                }
                case PathCommand::CubicTo: {
                    i_pts-=3;
                    break;
                }
            }
            if (src.cmds[i-1] == PathCommand::Close) break;
            i--;
        }
        t_stencil->cmdCnt = src.cmdCnt - i;
        t_stencil->ptsCnt = src.ptsCnt - i_pts;
        t_stencil->reserveCmd(src.cmdCnt - i);
        t_stencil->reservePts(src.ptsCnt - i_pts);
        memcpy(t_stencil->cmds, src.cmds + i, sizeof(PathCommand) * (src.cmdCnt - i));
        memcpy(t_stencil->pts, src.pts + i_pts, sizeof(Point) * (src.ptsCnt - i_pts));
    }

    void getPoints(ShapePath& path, uint32_t i_cmd, uint32_t i_pts, Point* point)
    {
        switch(path.cmds[i_cmd]){
            case PathCommand::Close: {
                point[0] = {path.pts[i_pts].x, path.pts[i_pts].y};
                point[1] = {path.pts[0].x, path.pts[0].y};
                break;
            }
            case PathCommand::MoveTo: {
                point[0] = {path.pts[i_pts].x, path.pts[i_pts].y};
                break;
            }
            case PathCommand::LineTo: {
                point[0] = {path.pts[i_pts].x, path.pts[i_pts].y};
                point[1] = {path.pts[i_pts+1].x, path.pts[i_pts+1].y};
                break;
            }
            case PathCommand::CubicTo: {
                point[0] = {path.pts[i_pts].x, path.pts[i_pts].y};
                point[1] = {path.pts[i_pts+1].x, path.pts[i_pts+1].y};
                point[2] = {path.pts[i_pts+2].x, path.pts[i_pts+2].y};
                point[3] = {path.pts[i_pts+3].x, path.pts[i_pts+3].y};
            }
        }
    }

    uint32_t getSegmentSegment(Point* seg1, Point* seg2, Intersection* intersection)
    {
        
    }

};

struct ShapeStroke
{
    float width = 0;
    uint8_t color[4] = {0, 0, 0, 0};
    float* dashPattern = nullptr;
    uint32_t dashCnt = 0;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;
    Stencil* stencil = nullptr;

    ShapeStroke() {}

    ShapeStroke(const ShapeStroke* src)
    {
        width = src->width;
        dashCnt = src->dashCnt;
        cap = src->cap;
        join = src->join;
        memcpy(color, src->color, sizeof(color));
        dashPattern = static_cast<float*>(malloc(sizeof(float) * dashCnt));
        memcpy(dashPattern, src->dashPattern, sizeof(float) * dashCnt);
    }

    ~ShapeStroke()
    {
        if (dashPattern) free(dashPattern);
    }
};





struct Shape::Impl
{
    ShapePath *path = nullptr;
    Fill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a
    void *edata = nullptr;              //engine data
    Shape *shape = nullptr;
    uint32_t flag = RenderUpdateFlag::None;

    Impl(Shape* s) : path(new ShapePath), shape(s)
    {
    }

    ~Impl()
    {
        if (path) delete(path);
        if (fill) delete(fill);
        if (stroke) delete(stroke);
    }

    bool dispose(RenderMethod& renderer)
    {
        return renderer.dispose(*shape, edata);
    }

    bool render(RenderMethod& renderer)
    {
        return renderer.render(*shape, edata);
    }

    void* update(RenderMethod& renderer, const RenderTransform* transform, vector<Composite>& compList, RenderUpdateFlag pFlag)
    {
        this->edata = renderer.prepare(*shape, this->edata, transform, compList, static_cast<RenderUpdateFlag>(pFlag | flag));
        flag = RenderUpdateFlag::None;
        return this->edata;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!path) return false;
        return path->bounds(x, y, w, h);
    }

    bool strokeWidth(float width)
    {
        //TODO: Size Exception?

        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->width = width;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool stencil(ShapePath& src)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        // if there is no stencil (i.e. this is first countour that is added)
        // crete new stencil
        if (!stroke->stencil) stroke->stencil = new Stencil(src);
        else {
            // if stencil exists, update it with new contour
            stroke->stencil->Update(src);
        }


        return true;
    }

    bool strokeCap(StrokeCap cap)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->cap = cap;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeJoin(StrokeJoin join)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->join = join;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->color[0] = r;
        stroke->color[1] = g;
        stroke->color[2] = b;
        stroke->color[3] = a;

        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeDash(const float* pattern, uint32_t cnt)
    {
       if (!stroke) stroke = new ShapeStroke();
       if (!stroke) return false;

        if (stroke->dashCnt != cnt) {
            if (stroke->dashPattern) free(stroke->dashPattern);
            stroke->dashPattern = nullptr;
        }

        if (!stroke->dashPattern) stroke->dashPattern = static_cast<float*>(malloc(sizeof(float) * cnt));

        for (uint32_t i = 0; i < cnt; ++i)
            stroke->dashPattern[i] = pattern[i];

        stroke->dashCnt = cnt;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    void reset()
    {
        path->reset();

        if (fill) {
            delete(fill);
            fill = nullptr;
        }
        if (stroke) {
            delete(stroke);
            stroke = nullptr;
        }

        color[0] = color[1] = color[2] = color[3] = 0;

        flag = RenderUpdateFlag::All;
    }

    Paint* duplicate()
    {
        auto ret = Shape::gen();
        if (!ret) return nullptr;

        auto dup = ret.get()->pImpl;

        //Color
        memcpy(dup->color, color, sizeof(color));
        dup->flag = RenderUpdateFlag::Color;

        //Path
        if (path) {
            dup->path = new ShapePath(path);
            dup->flag |= RenderUpdateFlag::Path;
        }

        //Stroke
        if (stroke) {
            dup->stroke = new ShapeStroke(stroke);
            dup->flag |= RenderUpdateFlag::Stroke;
        }

        if (fill) {
            dup->fill = fill->duplicate();
            dup->flag |= RenderUpdateFlag::Gradient;
        }

        return ret.release();
    }
};

#endif //_TVG_SHAPE_IMPL_H_
