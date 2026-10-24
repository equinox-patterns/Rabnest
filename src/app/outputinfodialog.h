/************************************************************************
 **
 **  @file   outputinfodialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  What one output was asked for, and what it cost.
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

#ifndef OUTPUTINFODIALOG_H
#define OUTPUTINFODIALOG_H

#include "core/cutplan.h"

#include <QDialog>
#include <QString>
#include <QStringList>

class QVBoxLayout;

/// What a PRINT run was asked for, and what it produced.
///
/// **A print run is not a cutting plan, and this is where it showed most.** Its row in the dock is a
/// shell — no cloth, no poses, no fold — so a dialogue that answered it with the cutting plan's
/// questions answered every one of them with a zero. Here are the paper's own facts: the format, its
/// columns, the two spacings, the pages it prints and the pieces it lays.
struct PrintInfo
{
    /// The row's own name, « Sizes 32–46 - Print ».
    QString title;

    /// The format's label, as the print panel names it — « A4/Letter framed », « A0 Roll ».
    QString format;

    /// Sheets side by side. Shown for every format but the roll and the A0 sheet, which are one
    /// column wide by definition.
    int columns = 1;
    bool hasColumns = false;

    /// The two figures the run was nested with, in CENTIMETRES — the unit the panel shows them in,
    /// and the one the plan stores: the document's margin from the edge, and the space between two
    /// pieces.
    double marginCm = 0.0;
    double gapCm = 0.0;

    /// How many pages it prints, the cover page counted when it takes a sheet of its own.
    int pages = 0;
    bool coverPage = false;

    /// The time the laying out cost, in seconds — zero when it was not measured.
    double seconds = 0.0;

    /// One entry per piece, and how many sizes it is printed at.
    QStringList pieces;
    int sizes = 0;
};

/// The two questions a line of the dock cannot answer: what was this output ASKED for, and what
/// did it cost.
///
/// **Read-only, and only facts.** Every figure comes from the plan itself — the cloth, the two
/// spacings, the fold and the direction are copied there when the plan is made, precisely so that
/// this dialogue can be answered months later without the request that produced it. The result half
/// says what the run produced: how many pieces, how much cloth, and how long it took.
///
/// It is a dialogue and not a window of its own: like the import journey, it belongs to the window
/// it is opened from.
class OutputInfoDialog : public QDialog
{
    Q_OBJECT

public:
    /// A cutting plan, and — when the run asked for a pair — its LINING.
    ///
    /// **The pair is answered as one output with two halves**, because that is what she made: one
    /// row in the dock, one run, two cloths. The settings are the run's and are said once; the two
    /// widths, the two lengths and the two piece lists are said for each cloth, since they are the
    /// only thing the two do not share.
    explicit OutputInfoDialog(const CutPlan &plan, const CutPlan *lining = nullptr,
                              QWidget *parent = nullptr);
    explicit OutputInfoDialog(const PrintInfo &print, QWidget *parent = nullptr);

private:
    void Finish(QVBoxLayout *layout, const QString &error);
};

#endif  // OUTPUTINFODIALOG_H
