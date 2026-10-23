/************************************************************************
 **
 **  @file   exportmarksettings.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  What the assembly marks look like, and where they go — Ostara's settings.
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

#ifndef EXPORTMARKSETTINGS_H
#define EXPORTMARKSETTINGS_H

#include <QVector>
#include <QtGlobal>

/// The geometric shapes a mark can take. The custom SVG is not built yet.
enum class ExportMarkShape : qint8
{
    Circle,
    Square,
    Octagon
};

/// @brief One row of the mark-shape table: what a combo shows, what a recipe carries, and which
///        file draws it.
struct ExportMarkShapeModel
{
    const char *label{nullptr}; ///< what the combo shows — translatable
    const char *token{nullptr}; ///< what a recipe carries — never translated
    const char *icon{nullptr};  ///< the file name, inside the `:/export/` resource folder
};

/// @brief The shapes, in the order Ostara offers them.
auto ExportMarkShapes() -> const QVector<ExportMarkShapeModel> &;

/// @brief What the assembly marks look like, and where they go.
///
/// **Ostara's `OExportMarkSettings`, ported.**
///
/// A mark is meant to be **shared by the sheets it registers**: its centre sits on a corner or on
/// the middle of an edge, so the corner mark is divided into four equal parts and the edge marks
/// into two, one per sheet. Each sheet prints its own part, and the whole reappears once the sheets
/// are glued — which is why a mark is positioned from the plan and never from the sheet.
///
/// Only the framed formats have room inside the sheet for them; the other tiled models are
/// ignored, as the frame is.
struct ExportMarkSettings
{
    ExportMarkShape shape{ExportMarkShape::Circle};

    /// The mark's side, in millimetres — a square box; the shape is drawn inside it.
    qreal sizeMm{40.0};

    /// Opacity of the marks, in percent.
    int opacity{100};

    /// The three placements, chosen independently.
    bool onCorner{true};
    bool onVerticalCentre{false};
    bool onHorizontalCentre{false};
};

#endif // EXPORTMARKSETTINGS_H
