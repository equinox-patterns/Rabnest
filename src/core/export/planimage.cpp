/************************************************************************
 **
 **  @file   planimage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The arithmetic of an exported plan, and nothing else.
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

#include "core/export/planimage.h"

#include <algorithm>
#include <cmath>

namespace
{
/// The drawing's own resolution, and the one every scene pixel is measured at: 96 to the inch.
constexpr double kDrawDpi = 96.0;

/// The ceiling Ostara refuses at, and refuses for the same reason: an image of more than a hundred
/// million pixels is not a plan, it is a file no machine will open.
constexpr double kLargestPixels = 100.0 * 1000.0 * 1000.0;
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
auto PlanImage::Of(const QRectF &rectPx) -> Plan
{
    Plan plan;
    plan.rectPx = rectPx;
    plan.widthMm = MmOf(rectPx.width());
    plan.heightMm = MmOf(rectPx.height());
    return plan;
}

//---------------------------------------------------------------------------------------------------------------------
auto PlanImage::MmOf(double pixels) -> double
{
    return pixels * 25.4 / kDrawDpi;
}

//---------------------------------------------------------------------------------------------------------------------
auto PlanImage::PixelsOf(double millimetres, int dpi) -> double
{
    return millimetres * static_cast<double>(dpi) / 25.4;
}

//---------------------------------------------------------------------------------------------------------------------
auto PlanImage::SizeFor(const Plan &plan, double targetMm, Axis axis, int dpi) -> Target
{
    Target target;
    target.dpi = dpi;

    // A plan with no width or no height has no shape to keep, and the caller has already refused
    // that case; a division by zero here would answer an infinity rather than a size.
    const bool pinned = axis == Axis::Width;
    const double own = pinned ? plan.widthMm : plan.heightMm;
    const double other = pinned ? plan.heightMm : plan.widthMm;
    const double otherMm = own > 0.0 ? targetMm * other / own : targetMm;

    const int counted = std::max(1, static_cast<int>(std::lround(PixelsOf(targetMm, dpi))));
    const int deduced = std::max(1, static_cast<int>(std::lround(PixelsOf(otherMm, dpi))));

    target.sizePx = pinned ? QSize(counted, deduced) : QSize(deduced, counted);
    return target;
}

//---------------------------------------------------------------------------------------------------------------------
auto PlanImage::IsTooLarge(const Target &target) -> bool
{
    const double pixels = static_cast<double>(target.sizePx.width())
                          * static_cast<double>(target.sizePx.height());
    return pixels > kLargestPixels;
}
