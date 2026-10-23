/************************************************************************
 **
 **  @file   exportartboards.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's tiling, ported: one plan cut into the sheets it is printed on.
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

#include "exportartboards.h"

#include "printmetrics.h"

#include <QtMath>

#include <algorithm>
#include <limits>

namespace
{
/// How far above a whole number a row count may land before it is taken for the next one.
///
/// The count is a division, not a subtraction, and the two figures that should meet exactly are
/// converted from millimetres on one side — so a plan of exactly three rows can come out at
/// 3.0000000000000004, and a fourth, empty sheet would appear out of nothing. The tolerance sits
/// far above that rounding and far below any real fraction of a frame.
constexpr double kRowTolerance = 1e-9;
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportArtboards::Compute(qreal binWidthPx, qreal usedHeightPx, qreal frameHeightMm, int columns)
    -> ExportArtboards::Grid
{
    Grid grid;

    if (binWidthPx <= 0.0 || usedHeightPx <= 0.0 || columns < 1)
    {
        return grid;
    }

    const qreal cellHeightPx = PrintMetrics::MmToPx(frameHeightMm);
    if (cellHeightPx <= 0.0)
    {
        return grid; // a roll, the A0 sheet, or a frame typed down to nothing: nothing to cut
    }

    // The width is read off the bin rather than off a second reading of the frame: binWidthPx IS
    // the frame's small side times the columns — the print tab builds the document width that way
    // — so dividing it back puts the plan's right edge exactly on the content's, and leaves no
    // third figure to hold in agreement with the other two.
    grid.cellWidthPx = binWidthPx / static_cast<qreal>(columns);
    grid.cellHeightPx = cellHeightPx;
    grid.columns = columns;

    // A frame typed down to nothing would ask for a count no plan can hold: it is bounded to what
    // an int carries rather than left to overflow one.
    const double rowLimit = static_cast<double>(std::numeric_limits<int>::max());
    const double rowCount = qCeil(usedHeightPx / cellHeightPx - kRowTolerance);
    grid.rows = static_cast<int>(std::min(std::max(rowCount, 1.0), rowLimit));

    // A sheet is never cut short, so the last row is whole whatever the nesting left in it — and
    // the paper it is printed on can be no shorter than what is printed on it.
    grid.planRect = QRectF(0.0, 0.0, grid.cellWidthPx * grid.columns,
                           std::max(grid.cellHeightPx * grid.rows, static_cast<qreal>(usedHeightPx)));
    grid.valid = true;
    return grid;
}
