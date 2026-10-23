/************************************************************************
 **
 **  @file   plansheet.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The arithmetic of one page, and nothing else.
 **
 **  @copyright
 **  Copyright (C) 2026 Sophie Denys <contact@sophiedenys.com>
 **
 **  This program is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  This program is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "core/export/plansheet.h"

#include <algorithm>

//---------------------------------------------------------------------------------------------------------------------
auto PlanSheet::Of(const QSizeF &planPx, const QRectF &printablePx, double marginPx,
                   double captionPx) -> Sheet
{
    Sheet sheet;

    if (planPx.width() <= 0.0 || planPx.height() <= 0.0 || printablePx.width() <= 0.0
        || printablePx.height() <= 0.0)
    {
        return sheet;
    }

    // The air on the four sides first, then the band under the plan: both are taken FROM the area
    // the printer can reach, and neither is negotiable afterwards.
    const QRectF usable = printablePx.adjusted(marginPx, marginPx, -marginPx, -marginPx);
    if (usable.width() <= 0.0 || usable.height() <= 0.0)
    {
        return sheet;
    }

    sheet.captionRect = QRectF(usable.left(), usable.bottom() - captionPx, usable.width(), captionPx);

    // A gap between the drawing and its line, so the line is not read as part of the plan.
    const double gap = captionPx / 4.0;
    const double drawingHeight = usable.height() - captionPx - gap;
    if (drawingHeight <= 0.0)
    {
        return sheet;
    }

    const QRectF drawing(usable.left(), usable.top(), usable.width(), drawingHeight);

    // The scale is the SMALLER of the two that would fit: a plan holds its shape, and the side that
    // runs out first is the one that decides.
    sheet.scale = std::min(drawing.width() / planPx.width(), drawing.height() / planPx.height());

    const QSizeF drawn(planPx.width() * sheet.scale, planPx.height() * sheet.scale);
    sheet.planRect = QRectF(drawing.center().x() - drawn.width() / 2.0,
                            drawing.center().y() - drawn.height() / 2.0, drawn.width(),
                            drawn.height());
    return sheet;
}
