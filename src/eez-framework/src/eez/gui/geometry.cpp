/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/gui/geometry.h>

namespace eez {
namespace gui {

enum OutCode {
    Inside = 0,
    Left = 1,
    Right = 2,
    Bottom = 4,
    Top = 8,
};

// http://www.richardssoftware.net/2014/07/clipping-lines-to-rectangle-using-cohen.html
int computeOutCode(PointF p, RectangleF r) {
    int code = OutCode::Inside;

    if (p.x < r.left) code |= OutCode::Left;
    if (p.x > r.right) code |= OutCode::Right;
    if (p.y < r.top) code |= OutCode::Top;
    if (p.y > r.bottom) code |= OutCode::Bottom;

    return code;
}

bool calculateIntersection(RectangleF r, PointF p1, PointF p2, int clipTo, PointF &p) {
    auto dx = (p2.x - p1.x);
    auto dy = (p2.y - p1.y);

    auto slopeY = dx / dy; // slope to use for possibly-vertical lines
    auto slopeX = dy / dx; // slope to use for possibly-horizontal lines

    if (clipTo & OutCode::Top) {
        p.x = p1.x + slopeY * (r.top - p1.y);
        p.y = r.top;
        return true;
    }

    if (clipTo & OutCode::Bottom) {
        p.x = p1.x + slopeY * (r.bottom - p1.y);
        p.y = r.bottom;
        return true;
    }

    if (clipTo & OutCode::Right) {
        p.x = r.right;
        p.y = p1.y + slopeX * (r.right - p1.x);
        return true;
    }

    if (clipTo & OutCode::Left) {
        p.x = r.left;
        p.y = p1.y + slopeX * (r.left - p1.x);
        return true;
    }

    return false;
}

bool clipSegment(RectangleF r, PointF &p1, PointF &p2) {
    // classify the endpoints of the line
    auto outCodeP1 = computeOutCode(p1, r);
    auto outCodeP2 = computeOutCode(p2, r);
    auto accept = false;

    while (true) { // should only iterate twice, at most
        // Case 1:
        // both endpoints are within the clipping region
        if ((outCodeP1 | outCodeP2) == OutCode::Inside) {
            accept = true;
            break;
        }

        // Case 2:
        // both endpoints share an excluded region, impossible for a line between them to be within the clipping region
        if ((outCodeP1 & outCodeP2) != 0) {
            break;
        }

        // Case 3:
        // The endpoints are in different regions, and the segment is partially within the clipping rectangle

        // Select one of the endpoints outside the clipping rectangle
        auto outCode = outCodeP1 != OutCode::Inside ? outCodeP1 : outCodeP2;

        // calculate the intersection of the line with the clipping rectangle
        PointF p = { 0.0f, 0.0f } ;
        if (!calculateIntersection(r, p1, p2, outCode, p)) {
            break;
        }

        // update the point after clipping and recalculate outcode
        if (outCode == outCodeP1) {
            p1 = p;
            outCodeP1 = computeOutCode(p1, r);
        } else {
            p2 = p;
            outCodeP2 = computeOutCode(p2, r);
        }
    }
    // if clipping area contained a portion of the line
    if (accept) {
        return true;
    }

    // the line did not intersect the clipping area
    return false;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
