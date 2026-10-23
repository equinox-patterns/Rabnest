/************************************************************************
 **
 **  @file   exportsheets.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Which sheets of a tiled plan carry a piece, and therefore get paper — Ostara's own.
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

#ifndef EXPORTSHEETS_H
#define EXPORTSHEETS_H

#include <QPolygonF>
#include <QRectF>
#include <QVector>

#include "exportartboards.h"

/// @brief Which sheets of a tiled plan carry a piece, and therefore get paper.
///
/// **This file is Ostara's `OExportSheets`, ported.** The nesting lays out ONE continuous plan;
/// `ExportArtboards` cuts it into a full grid of sheets. What neither implies is that every sheet
/// of that grid is worth printing — the engine minimises a height and knows nothing of sheets, so
/// its last row is usually short and its right-hand cells are often empty. This is where that is
/// read.
///
/// **A sheet with nothing on it is not a sheet.** The rule is one sentence — a sheet is printed if
/// the outline of a piece meets it — and it is read from the placements rather than from the grid,
/// because the placements are what a hand can still move: emptying a sheet by hand drops it, and
/// putting a piece back on one brings it back. Nothing here re-runs the nesting, and nothing here
/// chooses: it reads a result that already exists.
///
/// The one adaptation: Ostara reads the outlines from `OExportFootprint::Outline`, Rabnest from
/// `CutPlan::PlacedOutline` — the formula each application already draws its pieces with. That is
/// why the caller passes the shapes in rather than the placements: the port keeps the rule and
/// takes the ink from the one place that knows how to place it.
namespace ExportSheets
{
/// @brief One unbroken stretch of a grid line, as inclusive cell indices.
///
/// A line of the tiling is drawn in the stretches that have a printed sheet beside them, and a
/// stretch that runs the whole way is the whole line: a plan with nothing missing draws exactly
/// the chords it has always drawn. The frame carries an opacity the user set, so a boundary drawn
/// twice — once from each side — would come out darker than the rest of it.
struct Run
{
    int first{0};
    int last{0};
};

struct Set
{
    int columns{0};
    int rows{0};

    /// Row-major, `columns` per row: each sheet's 1-based rank among the PRINTED sheets, in
    /// reading order — left to right, then top to bottom — and 0 when the sheet is not printed.
    ///
    /// Presence and numbering are deliberately one field and not two. A sheet's number is a rank
    /// and not a place (ExportPaginationForm::Number), so it cannot be known before it is known
    /// which sheets print; holding the two apart would be two places to get one answer wrong. On a
    /// plan with nothing missing every rank is `row * columns + col + 1`, which is what the mode
    /// has always written — so a full plan is unchanged to the digit.
    ///
    /// Always `columns * rows` long, and empty only on a set that covers no grid at all.
    QVector<int> ranks{};

    /// How many sheets are printed — never more than `columns * rows`.
    int count{0};

    auto IsPresent(int row, int col) const -> bool;

    /// The sheet's printed number, or 0 when it is not printed.
    auto RankOf(int row, int col) const -> int;

    /// @brief The stretches of the vertical boundary at @p boundary (0..columns) that have a
    ///        printed sheet beside them.
    auto VerticalEdges(int boundary) const -> QVector<Run>;

    /// @brief The same for the horizontal boundary at @p boundary (0..rows).
    auto HorizontalEdges(int boundary) const -> QVector<Run>;
};

/// @brief Every cell of a `columns × rows` grid, printed — a tiling with nothing behind it.
///
/// The editor's preview draws a demonstration grid of six sheets and has no run to read; this is
/// what it hands the painter. Its ranks are the positions, so its page numbers read exactly as a
/// whole run's do.
auto All(int columns, int rows) -> Set;

/// @brief Read which sheets of @p grid carry one of @p ink — the placed outlines, in the plan's
///        own coordinates.
///
/// A sheet is printed when an outline meets its rectangle — meets, not fits: a piece straddling a
/// seam prints on both sheets, which is what cutting there means. The outline is used and not its
/// bounding box, because a piece turned forty-five degrees has a box far larger than itself, and
/// on a narrow column that difference is a whole sheet of paper asked for nothing.
///
/// **@p reserved is a rectangle the plan keeps free and that still has to be printed** — the block
/// a cover page stands in. It is ink for this question and for this one alone: a sheet whose whole
/// content is the cover page must be printed, and the block is not a placement, so a plan that
/// happens to keep every piece out of cell (0,0) would otherwise not print the sheet its own cover
/// page is drawn on.
auto Compute(const ExportArtboards::Grid &grid, const QVector<QPolygonF> &ink,
             const QRectF &reserved = {}) -> Set;
} // namespace ExportSheets

#endif // EXPORTSHEETS_H
