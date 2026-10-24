/************************************************************************
 **
 **  @file   printrundialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   24/10/2026
 **
 **  @brief  What a print run is printed on, before the printer's own panel opens.
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

#ifndef PRINTRUNDIALOG_H
#define PRINTRUNDIALOG_H

#include "core/print/exportpaper.h"

#include <QDialog>
#include <QPageSize>
#include <QVector>

class QComboBox;

/// What a print run asks before the printer's panel: its paper, and nothing else.
///
/// **The paper is not a preference here, it is a consequence.** The sheets are already at their final
/// size, and they are glued to each other: printed on another paper, or shrunk to fit, the plan still
/// looks like a plan and no longer assembles. So the choices are the papers that hold the run's sheet
/// whole — one for A4, Letter, A3, Tabloid and A0 runs, two for the framed formats, whose sheet is ten
/// millimetres inside the paper — and when there is only one it is shown and not offered.
///
/// The scale is not a question either: the run is printed at 100 %, one sheet per page, and the note
/// says so, because a reader who does not know it will look for the setting that changes it.
class PrintRunDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param name      the run's own name, which says what it covers
    /// @param sheetMm   one sheet, in millimetres — what the whole run is laid out on
    /// @param sheets    how many sheets it prints, the cover page's own sheet included
    /// @param papers    the papers that hold it, at least one
    /// @param preferred which one to stand on when the choice opens — the machine's own paper, when
    ///                  it is one of them. A framed run fits two papers and only one of them is
    ///                  probably in the printer; proposing the wrong one costs a click and, if she
    ///                  does not look, a plan printed on the wrong sheet.
    PrintRunDialog(const QString &name, const QSizeF &sheetMm, int sheets,
                   const QVector<ExportPaper::Paper> &papers,
                   QPageSize::PageSizeId preferred = QPageSize::A4, QWidget *parent = nullptr);

    auto paper() const -> QPageSize::PageSizeId;

private:
    QVector<ExportPaper::Paper> m_papers;
    QComboBox *m_choice = nullptr;
};

#endif // PRINTRUNDIALOG_H
