/************************************************************************
 **
 **  @file   exportframesettings.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The frame drawn on each sheet, and the two scissors marks — Ostara's settings.
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

#ifndef EXPORTFRAMESETTINGS_H
#define EXPORTFRAMESETTINGS_H

#include <QVector>
#include <QtGlobal>

/// @brief The frame drawn on each sheet of a tiled plan, and the two scissors marks.
///
/// **Ostara's `OExportFrameSettings`, ported** — the same fields, the same defaults.
///
/// There is no inset to configure: an artboard *is* the print frame, so the line sits on the
/// sheet's own boundary. Only the framed formats show it — for A3, A4, Letter and Tabloid that
/// boundary is the paper's edge, and drawing it there would trace a second line over the first.
///
/// Lengths are in points, as the size styles are, and converted at render time by
/// `PrintDPI / 72`. Nothing here is a fraction of the sheet: a mark keeps its size from one format
/// to the next, which is the whole contract of the preview.
struct ExportFrameSettings
{
    /// Thickness of the frame's line, in points.
    qreal widthPt{1.0};

    /// Solid or dashed, and it is an explicit choice rather than the size styles' « empty pattern
    /// means solid » — the pattern below is remembered while the line is solid, so that trying one
    /// and coming back does not lose what was typed.
    bool dashed{false};

    /// The pattern, in points: one dash, one space.
    QVector<qreal> dashes{10.0, 5.0};

    /// Opacity of the frame's line, in percent.
    int frameOpacity{100};

    /// Which cuts the scissors hang on. Both are drawn by default.
    bool scissorsHorizontal{true};
    bool scissorsVertical{true};

    /// Opacity of the two scissors marks, in percent.
    int scissorsOpacity{100};
};

#endif // EXPORTFRAMESETTINGS_H
