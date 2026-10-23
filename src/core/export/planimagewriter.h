/************************************************************************
 **
 **  @file   planimagewriter.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  A plan written as an image — SVG or PNG, and the two are not the same drawing.
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

#ifndef PLANIMAGEWRITER_H
#define PLANIMAGEWRITER_H

#include "core/export/planimage.h"

#include <QString>

#include <functional>

class QPainter;

/// The two syntaxes a plan can leave as.
namespace PlanImageWriter
{
enum class Format
{
    Svg,
    Png
};

/// Draws the plan: from @p sourcePx, into @p targetPx, on @p painter.
///
/// ⚠️ **The writer does not know what a plan IS.** A scene would tie it to one way of drawing, and
/// the drawing is the canvas's business — a plan whose strokes are sized for the view would be
/// written with the wrong thickness at the file's own scale, and only the page knows that scale.
using Renderer = std::function<void(QPainter *painter, const QRectF &sourcePx,
                                   const QRectF &targetPx)>;

/// The suffix a file of @p format carries, without the dot.
auto Suffix(Format format) -> QString;

/// Writes @p sourcePx drawn by @p render into @p path, @p target's pixels being the page.
///
/// @param title what an SVG calls itself; the PNG carries no place for it
/// @return false when the file could not be written — the caller says so, in words
auto Write(const QString &path, const Renderer &render, const QRectF &sourcePx,
           const PlanImage::Target &target, Format format, const QString &title) -> bool;
}  // namespace PlanImageWriter

#endif  // PLANIMAGEWRITER_H
