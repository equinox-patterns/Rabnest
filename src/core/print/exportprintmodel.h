/************************************************************************
 **
 **  @file   exportprintmodel.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The paper a plan is printed on — Ostara's own table of print models, ported.
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

#ifndef EXPORTPRINTMODEL_H
#define EXPORTPRINTMODEL_H

#include "core/print/exportpaper.h"

#include <QString>
#include <QVector>

/// @brief How a print model lays a plan out: on a roll, or on a grid of sheets.
enum class ExportPrintShape : qint8
{
    Roll,
    Tiled
};

/// @brief Where the cover page of a format stands.
///
/// A property of the FORMAT and of nothing else. The page is 19 × 25.9 cm wherever it goes, and
/// whether that fits beside a plan or needs a sheet of its own is a fact about the sheet the plan
/// is printed on — never about the run, or two runs of one format would put the same page in two
/// places.
enum class CoverSeat : qint8
{
    None,      ///< no cover page — nothing reserved, nothing drawn, no page added
    InSheet,   ///< a block reserved at the plan's top-left corner; the page centred in it
    ExtraSheet ///< a sheet of its own, above row 1, column 1
};

/// @brief One row of Ostara's print-model table.
///
/// **This table is Ostara's `kModels`, ported whole** (`oexportprintmodel.cpp:49-63`): the same
/// labels, the same tokens, the same millimetres, the same declared `hasFrame`, the same cover
/// seat and the same `hasColumns`. A format is a fact about paper, and two tables of paper are two
/// answers waiting to disagree.
struct ExportPrintModel
{
    /// What the combo shows — translatable.
    const char *label{nullptr};

    /// What the payload's recipe carries — never translated.
    const char *token{nullptr};

    ExportPrintShape shape{ExportPrintShape::Roll};

    /// The sheet's two sides, in millimetres. `heightMm` is 0 for a roll, which is what makes
    /// `FrameHeightMm` 0 and tells the tiling there is nothing to cut.
    qreal widthMm{0.0};
    qreal heightMm{0.0};

    /// True for the two framed formats only. **Declared, never guessed from the dimensions**: a
    /// format is framed because the table says so, and a table that derived it would be one more
    /// reading of the same numbers.
    bool hasFrame{false};

    CoverSeat coverSeat{CoverSeat::InSheet};

    /// The pair of papers this format belongs to — what a run of it may be printed on.
    ///
    /// **The pair, not every paper big enough.** A framed A4/Letter sheet is 190 × 259 mm, which
    /// happens to fit inside an A3 and an A0 as well; printing it there would be a plan on a sheet
    /// of the wrong size in a tiling meant for another, and nobody would see it until the sheets did
    /// not meet. The format knows its paper; the paper is deduced from the sheet only WITHIN it.
    ExportPaper::Family family{ExportPaper::Family::A4Letter};

    /// False for the A0 sheet and for the roll: the format is exactly one column wide by
    /// definition, and offering a count would offer a plan that no such paper can hold.
    bool hasColumns{false};

    /// The count the format opens on, before the user has chosen one — the creator's counts:
    /// five for the A4/Letter tier, three for the A3/Tabloid one, and a count of one wherever
    /// there is no count at all. **Declared, never derived**, as `hasFrame` is: Ostara computes
    /// it as `ceil(841 / width)` (`oexporttiledformatwidget.cpp:96`), which lands on the same
    /// figure for A3, A4 and A4/Letter framed and on a different one for the three others
    /// (Tabloid, Letter, A3/Tabloid framed) — the table says the creator's, and the divergence
    /// is reported rather than smoothed over.
    int defaultColumns{1};

    /// False for the A0 sheet: it is in the table so that a payload naming it is understood, but
    /// it is not offered — there is no way to reach it from the interface.
    bool selectable{false};
};

/// @brief The models, in the order Ostara declares them.
auto ExportPrintModels() -> const QVector<ExportPrintModel> &;

/// @brief The model whose token is @p token, or null — a payload may name a format this build has
///        since dropped, and that is answered, not guessed.
auto FindExportPrintModel(const QString &token) -> const ExportPrintModel *;

#endif // EXPORTPRINTMODEL_H
