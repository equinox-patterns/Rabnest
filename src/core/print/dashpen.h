/************************************************************************
 **
 **  @file   dashpen.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  A dash pattern in points, turned into the pen Qt draws with — Ostara's own conversion.
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

#ifndef DASHPEN_H
#define DASHPEN_H

#include <QColor>
#include <QPen>
#include <QVector>
#include <QtGlobal>

/// **Ostara's `OGradedSizeStyles::ApplyToPen`, ported verbatim** (`ogratedsizestyles.cpp:137-160`).
///
/// The frame of a sheet is drawn with it, and the reason to keep the code rather than write another
/// is written in the original: a Qt dash pattern counts in multiples of the pen width, so a length
/// stated in points becomes `pt × pxPerPt / width`. A second implementation would put the dashes of
/// a seam slightly elsewhere, and a seam is the one line two sheets must agree on.
namespace DashPen
{
/// @brief A dash pattern in points, in pixels.
inline auto DashLengthsPx(const QVector<qreal> &dashesPt, qreal pxPerPt) -> QVector<qreal>
{
    QVector<qreal> pattern;
    pattern.reserve(dashesPt.size());
    for (qreal value : dashesPt)
    {
        pattern.append(value * pxPerPt);
    }
    return pattern;
}

/// @brief Set @p pen to @p color and to the pattern @p dashesPt — solid when the list is empty.
inline void ApplyToPen(QPen &pen, const QColor &color, const QVector<qreal> &dashesPt, qreal pxPerPt)
{
    pen.setColor(color);

    if (dashesPt.isEmpty())
    {
        pen.setStyle(Qt::SolidLine);
        pen.setCapStyle(Qt::SquareCap);
        return;
    }

    // A Qt dash pattern is in multiples of the pen width, so a pixel length becomes px / width. A
    // pen that says nothing about its width is one pixel wide.
    const qreal width = pen.widthF() > 0.0 ? pen.widthF() : 1.0;
    QVector<qreal> pattern = DashLengthsPx(dashesPt, pxPerPt);
    for (qreal &value : pattern)
    {
        value /= width;
    }

    pen.setStyle(Qt::CustomDashLine);
    pen.setDashPattern(pattern);
    pen.setCapStyle(Qt::FlatCap);
}
} // namespace DashPen

#endif // DASHPEN_H
