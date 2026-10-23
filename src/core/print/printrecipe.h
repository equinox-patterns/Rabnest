/************************************************************************
 **
 **  @file   printrecipe.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The print choices the PDF's creator made, read out of the payload's recipe.
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

#ifndef PRINTRECIPE_H
#define PRINTRECIPE_H

#include "exporttemplate.h"

#include <QByteArray>
#include <QString>

/// @brief The format line of the recipe — what paper the creator printed on.
///
/// **These are Ostara's own attributes** (`oexportrecipe.cpp:108-116`): `token`, `framed`,
/// `columns`, `documentWidthMm`, `frameHeightMm`, `marginMm`, `gapMm`. Nothing is derived from the
/// drawing, and nothing has to be guessed: the file states the paper it was laid out for.
struct PrintFormat
{
    /// The model's token — `A4LetterFramed`, `A0Roll`, … Empty when the recipe names none.
    QString token;

    /// True for the two framed formats: the frame and the scissors are drawn, and nothing else is.
    bool framed = false;

    /// Sheets side by side. The sheet's own width is `documentWidthMm / columns`, which is how a
    /// format this build does not know is still measured exactly.
    int columns = 1;

    /// The bin the print run was nested in, in millimetres — columns × the sheet's width.
    qreal documentWidthMm = 0.0;

    /// The sheet's long side, in millimetres. **Zero is a roll and the A0 sheet**: there is no
    /// sheet to cut, so there is no frame, no numbering and no marks.
    qreal frameHeightMm = 0.0;

    /// The document margin and the space between pieces the run was nested with, in millimetres.
    qreal marginMm = 5.0;
    qreal gapMm = 5.0;

    /// True when a `<format>` element was read at all.
    bool present = false;

    /// The sheet's own width, in millimetres — read back from the bin rather than from a table.
    auto SheetWidthMm() const -> qreal
    {
        return columns > 0 ? documentWidthMm / static_cast<qreal>(columns) : 0.0;
    }

    /// True when the plan is tiled: a frame height is what makes sheets, so zero means a roll.
    auto Tiled() const -> bool { return frameHeightMm > 0.0; }
};

/// @brief Everything the payload states about the paper and its decor.
struct PrintRecipe
{
    PrintFormat format;

    /// The creator's decor. Meaningless unless @ref hasDecor — a roll carries none, and Rabnest
    /// then draws its own (ExportTemplate::RabnestDefault).
    ExportTemplate decor;
    bool hasDecor = false;

    /// The cover page, as the SVG the payload carries — Ostara's `<exportCover>`, written whole.
    /// Empty when the PDF had none.
    QByteArray coverSvg;
};

/// @brief Read the recipe out of a payload.
///
/// Returns a recipe whose `format.present` is false when the file carries no `<exportRecipe>` at
/// all — an export made before the recipe existed, or one made with « Compatible with Rabnest »
/// off. Nothing is guessed in that case: the caller decides what paper to print on.
auto ReadPrintRecipe(const QByteArray &payload) -> PrintRecipe;

#endif // PRINTRECIPE_H
