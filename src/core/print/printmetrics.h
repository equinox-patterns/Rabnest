/************************************************************************
 **
 **  @file   printmetrics.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The one conversion the print code needs — millimetres to the pixels Ostara prints at.
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

#ifndef PRINTMETRICS_H
#define PRINTMETRICS_H

#include <QtGlobal>

/// Ostara's print unit, kept as it is there.
///
/// `PrintDPI` is 96 in Ostara (`defglobal.h:33`) and `ToPixel(mm, Unit::Mm)` is
/// `mm / 25.4 * 96`. Every length of the decor — a frame's thickness, a mark's side, a number's
/// size — crosses that one conversion and nothing else, so the two applications print the same
/// sheet at the same scale.
namespace PrintMetrics
{
constexpr qreal kPrintDpi = 96.0;

/// Millimetres to pixels, as Ostara's `ToPixel(mm, Unit::Mm)`.
inline auto MmToPx(qreal mm) -> qreal
{
    return mm / 25.4 * kPrintDpi;
}

/// Pixels to millimetres — the reading Ostara does with `FromPixel(px, Unit::Mm)`.
inline auto PxToMm(qreal px) -> qreal
{
    return px * 25.4 / kPrintDpi;
}

/// Points to pixels — the reading used by every length the template states in points: a frame's
/// thickness, a dash pattern.
inline auto PtToPx(qreal pt) -> qreal
{
    return pt / 72.0 * kPrintDpi;
}
} // namespace PrintMetrics

#endif // PRINTMETRICS_H
