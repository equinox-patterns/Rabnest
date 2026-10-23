/************************************************************************
 **
 **  @file   plansheet.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  One page holding one cutting plan, and the line that says what it is.
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

#ifndef PLANSHEET_H
#define PLANSHEET_H

#include <QRectF>
#include <QSizeF>

/// Where a plan goes on ONE page, and how big it gets there.
///
/// **A page is a different drawing from a file.** An image export keeps the plan's own scale; a page
/// accepts that it cannot, and shrinks the plan until it fits inside the area the printer can really
/// reach — minus a margin of air, because the area a driver announces and the area it inks are not
/// always the same. The scale that comes out is a figure the sheet has to STATE: a plan printed at
/// 1:6 and not saying so is a ruler that lies by six.
namespace PlanSheet
{
/// The page, decomposed: where the plan is drawn, where the line under it is written, and the scale
/// between the plan's own pixels and the page's.
struct Sheet
{
    QRectF planRect;     ///< the plan's seat, in the page's own coordinates
    QRectF captionRect;  ///< the band beneath it, where the size, the scale and the cloth are written
    double scale = 0.0;  ///< page pixels per scene pixel
};

/// Fits @p planPx into @p printablePx.
///
/// @param marginPx  air kept on the four sides, INSIDE the printable area — a driver that inks a
///                  millimetre past what it announced is not a rare driver
/// @param captionPx the band reserved under the plan; it is part of what the plan may not use
///
/// The plan keeps its shape and is centred in what is left. A plan or an area with no extent
/// answers an empty sheet rather than a division by zero.
auto Of(const QSizeF &planPx, const QRectF &printablePx, double marginPx, double captionPx) -> Sheet;
}  // namespace PlanSheet

#endif  // PLANSHEET_H
