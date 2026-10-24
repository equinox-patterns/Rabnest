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

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
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

    /// True when the run is to be drawn black and solid, without the pattern's own line — the box is
    /// only ever offered for a run of ONE size (see `SetSizeCount`).
    auto PlainOutline() const -> bool;

    /// What was ticked at step 1 — shown, never chosen: « Size range 40–42 », « Size 46 ».
    void SetSizesText(const QString &text);

    /// How many sizes were ticked. A run of one size is offered the plain black line; a run of
    /// several is not, because a colour and a dash are what tell two sizes apart.
    void SetSizeCount(int count);

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
    QCheckBox *m_plainOutline = nullptr;
    QLabel *m_plainNote = nullptr;
    QLabel *m_documentWidth = nullptr;
    QDoubleSpinBox *m_margin = nullptr;
    QDoubleSpinBox *m_gap = nullptr;

    /// The layout the rows are in — the column row is shown or hidden as a ROW, so that a field
    /// a format has no use for leaves neither a box nor a label behind.
    QFormLayout *m_form = nullptr;

    /// The two figures the run is nested with, in MILLIMETRES — the unit they are stored in, as
    /// everywhere in the socle. The spin boxes only ever show them, in the unit the user reads.
    double m_marginMm = 5.0;
    double m_gapMm = 5.0;

    /// Whether the column count is the user's. A format's own count is written on the field
    /// until she types one, and a count she typed outlives the format she typed it on — the two
    /// rules Ostara states in one flag (`oexporttiledformatwidget.cpp:74`, `:91`).
    bool m_columnsChosen = false;

    /// True when the run is over one size — the only case the plain black line is offered in.
    bool m_singleSize = false;
};

#endif // PRINTPANEL_H
