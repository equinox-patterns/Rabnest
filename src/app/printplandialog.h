/************************************************************************
 **
 **  @file   printplandialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The two things a printed plan asks: what paper, and which way round.
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

#ifndef PRINTPLANDIALOG_H
#define PRINTPLANDIALOG_H

#include <QDialog>
#include <QPageLayout>
#include <QSizeF>

class QComboBox;

/// What a printed cutting plan asks before the printer's own panel opens: the paper, and the way
/// round it goes.
///
/// **Two formats, because the plan is read, not filed.** A page of a 1,6 m plan at 1:6 is an overview
/// kept beside the machine; A4 and Letter are the two sheets a home printer has, and a list of forty
/// paper names would only hide that. Everything else — the printer, the copies, the quality, and the
/// margins the driver really inks — belongs to the system's own panel, which is the only thing that
/// knows them.
class PrintPlanDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Paper
    {
        A4,
        Letter
    };

    enum class Side
    {
        Auto,
        Portrait,
        Landscape
    };

    explicit PrintPlanDialog(QWidget *parent = nullptr);

    auto paper() const -> Paper;

    /// The way round to print, resolved: « auto » turns the sheet so that its long side runs with the
    /// plan's, which is worth a third more scale on a plan wider than it is tall.
    auto orientation(const QSizeF &planPx) const -> QPageLayout::Orientation;

private:
    QComboBox *m_paper = nullptr;
    QComboBox *m_side = nullptr;
};

#endif  // PRINTPLANDIALOG_H
