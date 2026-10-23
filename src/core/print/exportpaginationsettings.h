/************************************************************************
 **
 **  @file   exportpaginationsettings.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  What a page number is made of, and where it is written — Ostara's settings.
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

#ifndef EXPORTPAGINATIONSETTINGS_H
#define EXPORTPAGINATIONSETTINGS_H

#include <QString>
#include <QVector>
#include <QtGlobal>

/// @brief What a page number is made of.
///
/// Two forms and no third: letters alone are not offered. Letters run along the columns, so they
/// give out at twenty-six of them — a plan that long is the ordinary case, and a form that stops
/// counting halfway is not a form.
enum class ExportPaginationForm : qint8
{
    /// The sheet's rank among the PRINTED sheets, in reading order — left to right, then top to
    /// bottom. A rank and not a place: a sheet dropped from the plan takes its number with it, and
    /// every sheet after it moves up one, so the plan reads 1, 2, 3 with no gap.
    Number,

    /// The column's letter and the row's number. The letter is the column and the number is the
    /// row, so a sheet is *registered* rather than counted: A1, B1, C1 across, A2, B2, C2 below.
    NumberAndLetter
};

/// The tiling itself would take more: this is a limit of what a sheet can be **called**, not of how
/// many can be laid side by side. Capping it where the count is entered is what keeps a plan from
/// being built whose sheets carry numbers no one can read.
constexpr int kPaginationMaxColumns = 26;

/// Half a centimetre — chosen on 17-09-2026, and it is not a taste: a printer cannot put ink on the
/// last few millimetres of a sheet, so a number closer to the edge than this would be the one mark
/// of the decor that a print could lose.
constexpr qreal kPaginationCornerInsetMm = 5.0;

/// @brief One row of the form table: what a combo shows, and a recipe carries.
struct ExportPaginationFormModel
{
    const char *label{nullptr};
    const char *token{nullptr};
};

/// @brief The forms, in the order Ostara offers them.
auto ExportPaginationForms() -> const QVector<ExportPaginationFormModel> &;

/// @brief How one page number is written.
///
/// Four settings and no more — no colour: the number is black, and there is nothing to configure.
/// Every setting therefore carries a real value, and the four below are what the number is until
/// another one is chosen.
struct ExportPaginationTextStyle
{
    /// Empty leaves the default face in place.
    QString fontFamily;

    /// The FACE the number is set in — a name, never a number. Empty is the family's Regular.
    ///
    /// ⚠️ A recipe written before the faces were named holds a **weight** in the same attribute:
    /// the painter resolves it, so an old file keeps the face it was drawn in.
    QString fontFace;

    /// The size, in pixels. An absolute length, as every size of the decor is: 96 pixels make an
    /// inch whatever the sheet, so the number keeps its size from a small artboard to a large one.
    int sizePx{16};

    /// Opacity of the number, in percent.
    int opacity{100};
};

/// @brief One placement of the page number: whether it is written, and how.
struct ExportPaginationPlacement
{
    bool enabled{false};
    ExportPaginationTextStyle style;
};

/// @brief What the pagination writes, and where.
///
/// The number depends on nothing but the sheet's own place in the tiling, which is why one template
/// serves every tiled format and nothing here names one.
struct ExportPaginationSettings
{
    ExportPaginationForm form{ExportPaginationForm::Number};

    /// The centre of each sheet.
    ExportPaginationPlacement centre;

    /// Inside each corner of each sheet, at kPaginationCornerInsetMm from the two edges it hugs.
    ExportPaginationPlacement corner;

    /// The seams running down the plan, at the middle of a sheet's height.
    ExportPaginationPlacement verticalCentre;

    /// The seams running across the plan, at the middle of a sheet's width.
    ExportPaginationPlacement horizontalCentre;
};

#endif // EXPORTPAGINATIONSETTINGS_H
