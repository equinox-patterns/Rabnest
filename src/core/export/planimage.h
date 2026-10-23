/************************************************************************
 **
 **  @file   planimage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The arithmetic of an exported plan: a rectangle, millimetres, pixels.
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

#ifndef PLANIMAGE_H
#define PLANIMAGE_H

#include <QRectF>
#include <QSize>

/// The size an exported plan is asked for, and the image that size becomes.
///
/// **The size is a LENGTH, never a scale factor.** The user says "make this plan 140 cm wide", and
/// the pixels follow from the resolution — which is the only one of the two a PNG can carry and the
/// only one an SVG ignores. Ostara's export works this way (`OExportPlanImage`), and a Rabnest plan
/// that left here at 73 % of what was asked because the arithmetic lived in two places would be the
/// same defect the rest of this file exists to prevent.
namespace PlanImage
{
/// Which of the plan's two sides the user pinned. The other follows from the shape of the plan.
enum class Axis
{
    Width,
    Height
};

/// The plan as it goes on the paper: its rectangle in scene pixels, and that rectangle in
/// millimetres.
///
/// ⚠️ **The rectangle is the PLAN, not the scene.** The canvas keeps twenty pixels of margin around
/// the drawing so the cloth's edge is not glued to the frame of the view; a file that carried them
/// would grow a white border nobody asked for, and the size the user typed would be the size of the
/// margin too.
struct Plan
{
    QRectF rectPx;
    double widthMm = 0.0;
    double heightMm = 0.0;
};

/// What the file will measure, in pixels, at the resolution asked for.
struct Target
{
    QSize sizePx;
    int dpi = 96;
};

/// The plan a rectangle describes, with its millimetres worked out at the drawing's own resolution.
auto Of(const QRectF &rectPx) -> Plan;

/// The millimetres a length of scene pixels is worth — 96 pixels to the inch, as everywhere.
auto MmOf(double pixels) -> double;

/// The pixels a length in millimetres is worth at @p dpi.
auto PixelsOf(double millimetres, int dpi) -> double;

/// The image @p plan becomes when @p targetMm of @p axis is asked for at @p dpi.
auto SizeFor(const Plan &plan, double targetMm, Axis axis, int dpi) -> Target;

/// Whether @p target would be an image no machine can hold — the same ceiling Ostara refuses at.
auto IsTooLarge(const Target &target) -> bool;
}  // namespace PlanImage

#endif  // PLANIMAGE_H
