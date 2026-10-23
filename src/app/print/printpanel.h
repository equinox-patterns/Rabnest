/************************************************************************
 **
 **  @file   printpanel.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's Print tab, as Rabnest applies it: the paper, and nothing that belongs to
 **          the nesting the user has already asked for.
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

#ifndef PRINTPANEL_H
#define PRINTPANEL_H

#include "core/print/exportprintmodel.h"

#include <QString>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

/// The print settings of one output.
///
/// **Ostara's Print tab, field for field**, minus the four controls that belong to the nesting the
/// user has already asked for — `Generate`, the size range, the measurement axes — and minus the
/// two rows Rabnest has no use for: a decor template (it has no template folder; the decor comes
/// from the payload) and a cover page template (the page is the imported one, drawn as it is).
///
/// What is left is what decides the paper and the run:
///
/// - the **unit** the figures are read in — a unit of reading, never of result;
/// - the **print model**, A0 excluded: it is not selectable in Ostara either;
/// - the **columns**, for the tiled formats, which is what makes the document's width;
/// - the **document margin** and the **space between pieces**, which go into the nesting;
/// - what the format states and nobody types: the sheet's two sides, and the document's width.
class PrintPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PrintPanel(QWidget *parent = nullptr);

    /// The model currently chosen, or null when the table does not know the token.
    auto Model() const -> const ExportPrintModel *;
    auto Columns() const -> int;
    auto MarginMm() const -> double;
    auto GapMm() const -> double;
    auto Unit() const -> int;

    /// What was ticked at step 1 — shown, never chosen: « Size range 40–42 », « Size 46 ».
    void SetSizesText(const QString &text);

    /// Sets the model by token, and the columns; the fields then say what the format says.
    void SetModel(const QString &token, int columns);

signals:
    /// One of the settings that changes the RUN moved — the model, the columns, the margin or the
    /// gap. The width of the document follows the sheet and the columns, as in Ostara.
    void Changed();

private:
    void Build();
    void UpdateFromModel();
    auto DocumentWidthMm() const -> double;

    QComboBox *m_unit = nullptr;
    QLabel *m_sizes = nullptr;
    QComboBox *m_model = nullptr;
    QLabel *m_sheetSize = nullptr;
    QSpinBox *m_columns = nullptr;
    QLabel *m_documentWidth = nullptr;
    QDoubleSpinBox *m_margin = nullptr;
    QDoubleSpinBox *m_gap = nullptr;

    /// The two figures the run is nested with, in MILLIMETRES — the unit they are stored in, as
    /// everywhere in the socle. The spin boxes only ever show them, in the unit the user reads.
    double m_marginMm = 5.0;
    double m_gapMm = 5.0;
};

#endif // PRINTPANEL_H
