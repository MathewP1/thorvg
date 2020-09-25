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

struct StrokeStencil
{
    struct Intersection
    {
        Point p;
        float t;
        uint32_t inner, outer, inner_pts, outer_pts;
    };
    // thats stroke stencil (outline)
    ShapePath* stencil = nullptr;

    // copy of contour we are trying to add to stroke stencil
    // needed to help with looking for intersections and splitting paths
    // t - temp
    ShapePath* t_stencil = nullptr;

    // copy first contour (up to first close) to stencil
    // copy everything else (if it exists - it should only exist if shape is added
    // with svg path, otherwise we update stencil after every close cmd) to temp stencil
    StrokeStencil(ShapePath& first)
    {
        stencil = new ShapePath(first);
        t_stencil = new ShapePath();
        all_intersections = nullptr;
    }
    ~StrokeStencil()
    {
        if (stencil) delete(stencil);
        if (t_stencil) delete(t_stencil);
        if (all_intersections) free(all_intersections);
    }

    void Update(ShapePath& src)
    {
        copyContour(src);

        //search for intersections
        printf("\nLooking for intersections...\n\n");
        if (!all_intersections) all_intersections = static_cast<Intersection*>(calloc(alloc_intersections, sizeof(Intersection)));

        Point outer_line[2], inner_line[2];
        Point outer_cubic[4], inner_cubic[4];
        PathCommand inner_cmd, outer_cmd;

        // init loop variables
        uint32_t i_outer = 0; uint32_t i_outer_pts = 0;
        uint32_t i_inner = 0; uint32_t i_inner_pts = 0;
        do{
            i_inner = i_inner_pts = 0;
            outer_cmd = stencil->cmds[i_outer];

            // check if we should skip this command
            if (outer_cmd == PathCommand::MoveTo) continue;
            if (outer_cmd == PathCommand::Close){
                getPoints(*stencil, i_outer, i_outer_pts, outer_line);
                if (outer_line[0].x == outer_line[1].x && outer_line[0].y == outer_line[1].y) continue;
            }

            // printf("Cmd: %u\n", stencil->cmds[i_outer]);
            if (outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo){
                getPoints(*stencil, i_outer, i_outer_pts, outer_line);
                i_outer_pts++;
            }
            if (outer_cmd == PathCommand::CubicTo){
                getPoints(*stencil, i_outer, i_outer_pts, outer_cubic);
                i_outer_pts+=3;
            }

            do{
                inner_cmd = t_stencil->cmds[i_inner];

                if (inner_cmd == PathCommand::MoveTo) continue;
                if (inner_cmd == PathCommand::Close){
                    getPoints(*t_stencil, i_inner, i_inner_pts, inner_line);
                    if (inner_line[0].x == inner_line[1].x && inner_line[0].y == inner_line[1].y) continue;
                }
                // printf("Cmd inner: %u\n", t_stencil->cmds[i_inner]);
                if (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo){
                    getPoints(*t_stencil, i_inner, i_inner_pts, inner_line);
                    i_inner_pts++;
                }
                if (inner_cmd == PathCommand::CubicTo){
                    getPoints(*t_stencil, i_inner, i_inner_pts, inner_cubic);
                    i_inner_pts+=3;
                }

                // look for intersections
                Intersection intersections[9];  // theoritical max number of intersections (cubic-cubic)
                uint32_t num_intersections = 0;

                // line line
                if ((outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo)
                    && (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo)){
                    printf("Comparing line (%f %f) (%f %f) with line (%f %f) (%f %f)\n",
                                outer_line[0].x, outer_line[0].y, outer_line[1].x, outer_line[1].y,
                                inner_line[0].x, inner_line[0].y, inner_line[1].x, inner_line[1].y);
                    num_intersections = getLineLineIntersection(outer_line, inner_line, intersections);
                }
                // line cubic
                if ((outer_cmd == PathCommand::Close || outer_cmd == PathCommand::LineTo)
                    && inner_cmd == PathCommand::CubicTo){
                    printf("Comparing line (%f %f) (%f %f) with cubic (%f %f) (%f %f) (%f %f) (%f %f)\n\n",
                               outer_line[0].x, outer_line[0].y, outer_line[1].x, outer_line[1].y,
                               inner_cubic[0].x, inner_cubic[0].y, inner_cubic[1].x, inner_cubic[1].y, inner_cubic[2].x, inner_cubic[2].y, inner_cubic[3].x, inner_cubic[3].y);
                }
                // cubic line
                if (outer_cmd == PathCommand::CubicTo
                    && (inner_cmd == PathCommand::Close || inner_cmd == PathCommand::LineTo)){
                    printf("Comparing cubic (%f %f) (%f %f) (%f %f) (%f %f) with line (%f %f) (%f %f)\n\n",
                               outer_cubic[0].x, outer_cubic[0].y, outer_cubic[1].x, outer_cubic[1].y, outer_cubic[2].x, outer_cubic[2].y, outer_cubic[3].x, outer_cubic[3].y,
                               inner_line[0].x, inner_line[0].y, inner_line[1].x, inner_line[1].y);
                }
                // cubic cubic
                if (outer_cmd == PathCommand::CubicTo && inner_cmd == PathCommand::CubicTo){
                    printf("Comparing cubic (%f %f) (%f %f) (%f %f) (%f %f) with cubic (%f %f) (%f %f) (%f %f) (%f %f)\n\n",
                               outer_cubic[0].x, outer_cubic[0].y, outer_cubic[1].x, outer_cubic[1].y, outer_cubic[2].x, outer_cubic[2].y, outer_cubic[3].x, outer_cubic[3].y,
                               inner_cubic[0].x, inner_cubic[0].y, inner_cubic[1].x, inner_cubic[1].y, inner_cubic[2].x, inner_cubic[2].y, inner_cubic[3].x, inner_cubic[3].y);
                }

                if (num_intersections){
                    printf("***Found %u intersections\n", num_intersections);
                    addIntersections(intersections, num_intersections, i_inner, i_outer, i_inner_pts, i_outer_pts);
                }
            }while(++i_inner < t_stencil->cmdCnt);
        }while(++i_outer < stencil->cmdCnt);

        for (uint32_t i = 0; i < num_all_intersections; i++){
            printf("Intersection at: (%f, %f), cmd outer: %u, cmd inner: %u\n", all_intersections[i].p.x, all_intersections[i].p.y, all_intersections[i].outer, all_intersections[i].inner);
        }
        _rebuildPath();

    }

private:
    Intersection* all_intersections;
    uint32_t num_all_intersections = 0;
    uint32_t alloc_intersections = 4;
    void _splitLine(ShapePath& path, uint32_t cmd_num, uint32_t pts_num, Point p)
    {
        // TODO: sth wrong with memmove
        printf("Before line split: \n");
        for( uint32_t i = 0; i < path.cmdCnt; i++){
            printf("Cmd: %u\n", path.cmds[i]);
        }
        for (uint32_t i = 0; i < path.ptsCnt; i++){
            printf("Point: (%f %f)\n", path.pts[i].x, path.pts[i].y);
        }
        path.cmdCnt++;
        path.reserveCmd(path.cmdCnt);
        memmove(&path.cmds[cmd_num] + 1, &path.cmds[cmd_num], sizeof(PathCommand) * (path.cmdCnt - cmd_num));
        path.cmds[cmd_num] = PathCommand::CubicTo;

        path.ptsCnt++;
        path.reservePts(path.ptsCnt);
        memmove(&path.pts[pts_num] + 1, &path.pts[pts_num], sizeof(Point*) * (path.ptsCnt - pts_num));
        path.pts[pts_num] = {999999, 9999999};
        printf("After line split: \n");
        for( uint32_t i = 0; i < path.cmdCnt; i++){
            printf("Cmd: %u\n", path.cmds[i]);
        }
        for (uint32_t i = 0; i < path.ptsCnt; i++){
            printf("Point: (%f %f)\n", path.pts[i].x, path.pts[i].y);
        }
    }

    void _rebuildPath()
    {
        while (num_all_intersections-- > 0)
        {
            switch(stencil->cmds[all_intersections->inner]){
                case PathCommand::LineTo: {
                    _splitLine(*t_stencil, all_intersections->inner, all_intersections->inner_pts, all_intersections->p);
                    break;
                }
            }
            switch(t_stencil->cmds[all_intersections->outer]){
                case PathCommand::LineTo: {
                    _splitLine(*stencil, all_intersections->outer, all_intersections->outer_pts, all_intersections->p);
                    break;
                }
            }
            ++all_intersections;
        }
    }
    void _getLineEquation(Point &p1, Point &p2, float &a, float &b, float &c)
    {
        a = p2.y - p1.y;
        b = p1.x - p2.x;
        c = a * p1.x + b * p1.y;
    }

    bool _LineLine(Point &p1, Point &p2, Point &p3, Point &p4, Intersection &p, bool &overlap)
    {
        overlap = false;
        float a1, b1, c1, a2, b2, c2;
        _getLineEquation(p1, p2, a1, b1, c1);
        _getLineEquation(p3, p4, a2, b2, c2);
        float det = a1*b2 - a2*b1;
        if (abs(det) < FLT_EPSILON){  //lines are parallel, check for overlap
            float det1 = a1*c2 - a2*c1;
            float det2 = b1*c2 - b2*c1;
            if (abs(det1) < FLT_EPSILON && abs(det2) < FLT_EPSILON){
                overlap = true;
                return true;
            } else return false;
        }
        p.p.x = (c1*b2 - c2*b1)/det;
        if ((max(min(p1.x, p2.x), min(p3.x, p4.x)) <= p.p.x) && (p.p.x <= min(max(p1.x, p2.x), max(p3.x, p4.x))))
        {
            p.p.y = (a1*c2 - a2*c1)/det;
            if ((max(min(p1.y, p2.y), min(p3.y, p4.y)) <= p.p.y) && (p.p.y <= min(max(p1.y, p2.y), max(p3.y, p4.y)))) return true;
        }
        return false;
    }

    void addIntersections(Intersection* intersections, uint32_t num_intersections, uint32_t inner, uint32_t outer, uint32_t inner_pts, uint32_t outer_pts)
    {
        if (num_all_intersections + num_intersections > alloc_intersections)
        {
            alloc_intersections = num_all_intersections + num_intersections;
            all_intersections = static_cast<Intersection*>(realloc(all_intersections, sizeof(Intersection) * alloc_intersections));
        }
        for (uint32_t i = 0; i < num_intersections; i++){
            all_intersections[num_all_intersections].p = intersections[i].p;
            all_intersections[num_all_intersections].t = intersections[i].t;
            all_intersections[num_all_intersections].inner = inner;
            all_intersections[num_all_intersections].outer = outer;
            all_intersections[num_all_intersections].inner_pts = inner_pts;
            all_intersections[num_all_intersections].outer_pts = outer_pts;

            num_all_intersections++;
        }
    }

    uint32_t getLineLineIntersection(Point* line1, Point* line2, Intersection* intersection)
    {
        bool overlap, result;
        result = _LineLine(line1[0], line1[1], line2[0], line2[1], *intersection, overlap);
        if (result) return 1;
        return 0;
    }

    uint32_t getLineCubicIntersection(Point* line, Point* cubic, Point* intersection)
    {
        tvg::Bezier cub;
        cub.start.x = cubic[0].x; cub.start.y = cubic[0].y;
        cub.ctrl1.x = cubic[1].x; cub.ctrl1.y = cubic[1].y;
        cub.ctrl2.x = cubic[2].x; cub.ctrl2.y = cubic[2].y;
        cub.end.x = cubic[3].x; cub.end.y = cubic[3].y;

        // return _LineCubic(cub, line[0], line[1], intersection);
        return 0;
    }

    uint32_t getCubicCubicIntersection(Point* cubic1, Point* cubic2, Point* intersection)
    {
        tvg::Bezier cub1;
        cub1.start.x = cubic1[0].x; cub1.start.y = cubic1[0].y;
        cub1.ctrl1.x = cubic1[1].x; cub1.ctrl1.y = cubic1[1].y;
        cub1.ctrl2.x = cubic1[2].x; cub1.ctrl2.y = cubic1[2].y;
        cub1.end.x = cubic1[3].x; cub1.end.y = cubic1[3].y;

        tvg::Bezier cub2;
        cub2.start.x = cubic2[0].x; cub2.start.y = cubic2[0].y;
        cub2.ctrl1.x = cubic2[1].x; cub2.ctrl1.y = cubic2[1].y;
        cub2.ctrl2.x = cubic2[2].x; cub2.ctrl2.y = cubic2[2].y;
        cub2.end.x = cubic2[3].x; cub2.end.y = cubic2[3].y;

        return 0;
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

    void copyContour(ShapePath& src)
    {
        // find last contour (between last and previous to last close cmds)
        printf("In StrokeStencil::Update\n\n");
        for (uint32_t i = 0; i < src.cmdCnt; i++){
            printf("Cmd: %u\n", src.cmds[i]);
        }

        printf("Cmdcnt: %u\n", src.cmdCnt);
        printf("PtsCnt: %u\n", src.ptsCnt);
        int i = src.cmdCnt - 2; // start looping at previus to last, last cmd should be close
        int i_pts = src.ptsCnt;
        while(i - 1 >= 0){
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
            if(src.cmds[i - 1] == PathCommand::Close) break;
            i--;
        }
        printf("Cmd index: %i, Pts index: %i\n", i, i_pts);
        // copy rest starting at found indexes
        // use memcpy or memmove here, as we know indexes and don't need to manually iterate
        t_stencil->cmdCnt = src.cmdCnt - i;
        t_stencil->ptsCnt = src.ptsCnt - i_pts;
        t_stencil->reserveCmd(src.cmdCnt - i);
        t_stencil->reservePts(src.ptsCnt - i_pts);
        memcpy(t_stencil->cmds, src.cmds + i, sizeof(PathCommand) * (src.cmdCnt - i));
        memcpy(t_stencil->pts, src.pts + i_pts, sizeof(Point) * (src.ptsCnt - i_pts));

        printf("All commands: \n");
        for (uint32_t i = 0; i < src.cmdCnt; i++){
            printf("cmd: %u\n", src.cmds[i]);
        }
        printf("All points:\n");
        for (uint32_t i = 0; i < src.ptsCnt; i++){
            printf("point: (%f %f)\n", src.pts[i].x, src.pts[i].y);
        }
        printf("\n\n");
        printf("Copied commands\n");
        for (uint32_t i = 0; i < t_stencil->cmdCnt; i++){
            printf("cmd: %u\n", t_stencil->cmds[i]);
        }
        printf("Copied points\n");
        for (uint32_t i = 0; i < t_stencil->ptsCnt; i++){
            printf("Point: (%f %f)\n", t_stencil->pts[i].x, t_stencil->pts[i].y);
        }
    }
};

struct ShapeStroke
{
    StrokeStencil* stencil = nullptr;

    float width = 0;
    uint8_t color[4] = {0, 0, 0, 0};
    float* dashPattern = nullptr;
    uint32_t dashCnt = 0;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;

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
        // if (stencil) delete(stencil);
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

    bool update(RenderMethod& renderer, const RenderTransform* transform, RenderUpdateFlag pFlag)
    {
        edata = renderer.prepare(*shape, edata, transform, static_cast<RenderUpdateFlag>(pFlag | flag));

        flag = RenderUpdateFlag::None;

        if (edata) return true;
        return false;
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

        if (!stroke->stencil) stroke->stencil = new StrokeStencil(src);
        else {
            // update stencil with new shape
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
