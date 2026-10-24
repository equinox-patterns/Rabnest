/************************************************************************
 **
 **  @file   printpanel.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's Print tab, built field for field.
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

#include "app/print/printpanel.h"
#include "app/factorywidgets.h"

#include "core/print/exportpaginationsettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>

#include <cmath>

namespace
{
/// The unit a figure is READ in. A unit of reading, never of result: every length is stored in
/// millimetres and shown in whichever of the three the user picked, as in Ostara.
enum class DisplayUnit : qint8
{
    Millimetre,
    Centimetre,
    Inch
};

auto UnitLabel(DisplayUnit unit) -> QString
{
    switch (unit)
    {
        case DisplayUnit::Millimetre:
            return QStringLiteral("mm");
        case DisplayUnit::Centimetre:
            return QStringLiteral("cm");
        case DisplayUnit::Inch:
            return QStringLiteral("inch");
    }
    return QStringLiteral("cm");
}

/// Millimetres to the unit, and back. The three factors are Ostara's (`def.cpp:624-639`).
auto FromMm(double mm, DisplayUnit unit) -> double
{
    switch (unit)
    {
        case DisplayUnit::Millimetre:
            return mm;
        case DisplayUnit::Centimetre:
            return mm / 10.0;
        case DisplayUnit::Inch:
            return mm / 25.4;
    }
    return mm;
}

auto ToMm(double value, DisplayUnit unit) -> double
{
    switch (unit)
    {
        case DisplayUnit::Millimetre:
            return value;
        case DisplayUnit::Centimetre:
            return value * 10.0;
        case DisplayUnit::Inch:
            return value * 25.4;
    }
    return value;
}

auto CurrentUnit(const QComboBox *combo) -> DisplayUnit
{
    return static_cast<DisplayUnit>(combo->currentData().toInt());
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
PrintPanel::PrintPanel(QWidget *parent) : QWidget(parent)
{
    Build();
}

//---------------------------------------------------------------------------------------------------------------------
void PrintPanel::Build()
{
    auto *form = new QFormLayout(this);
    form->setContentsMargins(12, 8, 12, 8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_form = form;

    // ── The unit ───────────────────────────────────────────────────────────────────────────────
    m_unit = MakeComboBox(this);
    m_unit->addItem(UnitLabel(DisplayUnit::Millimetre), static_cast<int>(DisplayUnit::Millimetre));
    m_unit->addItem(UnitLabel(DisplayUnit::Centimetre), static_cast<int>(DisplayUnit::Centimetre));
    m_unit->addItem(UnitLabel(DisplayUnit::Inch), static_cast<int>(DisplayUnit::Inch));
    m_unit->setCurrentIndex(1); // centimetres, as Ostara opens
    form->addRow(tr("Unit:"), m_unit);

    // ── What was ticked at step 1, as a reminder ────────────────────────────────────────────────
    m_sizes = new QLabel(this);
    m_sizes->setWordWrap(true);
    form->addRow(tr("Generate:"), m_sizes);

    // ── The paper ───────────────────────────────────────────────────────────────────────────────
    m_model = MakeComboBox(this);
    for (const ExportPrintModel &model : ExportPrintModels())
    {
        // A0 is in the table and NOT offered, exactly as Ostara lists it greyed: the row exists so
        // that a payload naming it is understood, not so that a run can reach it.
        if (!model.selectable)
        {
            continue;
        }

        m_model->addItem(QCoreApplication::translate("OExportPrintModel", model.label),
                         QString::fromLatin1(model.token));
    }
    form->addRow(tr("Print model:"), m_model);

    m_sheetSize = new QLabel(this);
    form->addRow(QString(), m_sheetSize);

    m_columns = new QSpinBox(this);
    m_columns->setRange(1, kPaginationMaxColumns);
    form->addRow(tr("Columns:"), m_columns);

    m_documentWidth = new QLabel(this);
    form->addRow(tr("Document width:"), m_documentWidth);

    m_margin = new QDoubleSpinBox(this);
    m_margin->setDecimals(2);
    m_margin->setRange(0.0, 1000.0);
    m_margin->setSingleStep(0.1);
    m_margin->setValue(0.5);
    form->addRow(tr("Document margin:"), m_margin);

    m_gap = new QDoubleSpinBox(this);
    m_gap->setDecimals(2);
    m_gap->setRange(0.0, 1000.0);
    m_gap->setSingleStep(0.1);
    m_gap->setValue(0.5);
    form->addRow(tr("Space between pieces:"), m_gap);

    // ── Le tracé, quand le run n'a qu'une taille ────────────────────────────────────────────────
    //
    // **En DERNIER, et coché.** Dernier parce que c'est une question sur le DESSIN, pas sur la
    // feuille : elle vient après tout ce qui décide de la mise en page. Cochée parce qu'une taille
    // seule n'a rien à distinguer par sa couleur — le tracé du patron ne fait alors que dessiner des
    // pointillés fins qui disparaissent à l'échelle où le plan est montré. Les mots sont simples à
    // dessein : c'est une question sur une image, pas sur un style.
    m_plainOutline = new QCheckBox(tr("Use a plain black outline instead of the pattern's"), this);
    m_plainOutline->setChecked(true);
    form->addRow(m_plainOutline);

    // ⚠️ **Ce que le canevas fait de TOUS les tracés, dit à l'endroit où l'on s'en étonne.** Sur le
    // dessin, un pointillé de patron est plus fin qu'un pixel ; sur le papier il est juste. La phrase
    // est là pour que personne ne croie que les pointillés sont perdus.
    m_plainNote = new QLabel(this);
    m_plainNote->setObjectName(QStringLiteral("outputDescription"));
    m_plainNote->setWordWrap(true);
    m_plainNote->setContentsMargins(24, 0, 0, 0);
    form->addRow(m_plainNote);

    connect(m_plainOutline, &QCheckBox::toggled, this, [this] { emit Changed(); });

    connect(m_unit, &QComboBox::currentIndexChanged, this, [this] {
        UpdateFromModel();
    });
    connect(m_model, &QComboBox::currentIndexChanged, this, [this] {
        UpdateFromModel();
        emit Changed();
    });
    connect(m_columns, &QSpinBox::valueChanged, this, [this] {
        // A count typed is the user's from here on: no format changed afterwards rewrites it,
        // and the plan keeps the width she asked for.
        m_columnsChosen = true;
        UpdateFromModel();
        emit Changed();
    });
    connect(m_margin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        m_marginMm = ToMm(value, CurrentUnit(m_unit));
        emit Changed();
    });
    connect(m_gap, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        m_gapMm = ToMm(value, CurrentUnit(m_unit));
        emit Changed();
    });

    UpdateFromModel();
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::Model() const -> const ExportPrintModel *
{
    return FindExportPrintModel(m_model->currentData().toString());
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::Columns() const -> int
{
    const ExportPrintModel *model = Model();
    if (model == nullptr || !model->hasColumns)
    {
        return 1;
    }
    return m_columns->value();
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::MarginMm() const -> double
{
    return m_marginMm;
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::GapMm() const -> double
{
    return m_gapMm;
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::PlainOutline() const -> bool
{
    // A run of several sizes has no plain line, whatever the box last said: it would draw them all
    // the same and they could no longer be told apart.
    return m_singleSize && m_plainOutline->isChecked();
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::Unit() const -> int
{
    return static_cast<int>(CurrentUnit(m_unit));
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPanel::DocumentWidthMm() const -> double
{
    const ExportPrintModel *model = Model();
    if (model == nullptr)
    {
        return 0.0;
    }
    return model->widthMm * static_cast<double>(Columns());
}

//---------------------------------------------------------------------------------------------------------------------
void PrintPanel::SetSizesText(const QString &text)
{
    m_sizes->setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void PrintPanel::SetSizeCount(int count)
{
    m_singleSize = count == 1;

    // ⚠️ **The sentence is shown ALWAYS, the box only for one size.** The canvas draws every run with
    // plain lines — that is what the sentence explains — so it belongs to every run. Only the
    // sentence's last clause speaks of the box, and it is only there when the box is.
    m_form->setRowVisible(m_plainOutline, m_singleSize);
    m_form->setRowVisible(m_plainNote, true);

    m_plainNote->setText(m_singleSize
                             ? tr("On screen, every size is drawn with a plain line so the plan stays "
                                  "readable. The export uses the pattern's own dashes, unless the "
                                  "plain black outline above is asked for.",
                                  "note under the plain-outline box")
                             : tr("On screen, every size is drawn with a plain line so the plan stays "
                                  "readable. The export uses each size's own dashes.",
                                  "note about the canvas, when the run has several sizes"));
}

//---------------------------------------------------------------------------------------------------------------------
void PrintPanel::SetModel(const QString &token, int columns)
{
    // Setting the panel up is not the user changing it: without this, opening the panel on the
    // recipe's model would re-lay the whole plan twice before the canvas had been shown once.
    const QSignalBlocker blockModel(m_model);
    const QSignalBlocker blockColumns(m_columns);
    const QSignalBlocker blockUnit(m_unit);

    const int index = m_model->findData(token);
    if (index >= 0)
    {
        // Setting the model first, then the columns: UpdateFromModel recomputes the columns on its
        // own when the model changes, and a column count restored afterwards is the creator's.
        m_model->setCurrentIndex(index);
    }

    // A count the run was nested with is a chosen one: it survives the model it is put on, which
    // is what the flag means everywhere else in this panel.
    m_columnsChosen = columns > 0;
    m_columns->setValue(columns > 0 ? columns : m_columns->value());
    UpdateFromModel();
}

//---------------------------------------------------------------------------------------------------------------------
void PrintPanel::UpdateFromModel()
{
    const ExportPrintModel *model = Model();
    const DisplayUnit unit = CurrentUnit(m_unit);

    // The unit is a unit of READING: the stored millimetres do not move, and the spin boxes are
    // rewritten in the unit now in force. Signals are blocked because showing a figure is not the
    // user changing one, and a Changed() emitted from here would re-run the nesting for a suffix.
    const QSignalBlocker blockMargin(m_margin);
    const QSignalBlocker blockGap(m_gap);
    const QSignalBlocker blockColumns(m_columns);

    m_margin->setSuffix(QStringLiteral(" ") + UnitLabel(unit));
    m_gap->setSuffix(QStringLiteral(" ") + UnitLabel(unit));
    m_margin->setValue(FromMm(m_marginMm, unit));
    m_gap->setValue(FromMm(m_gapMm, unit));

    if (model == nullptr)
    {
        m_sheetSize->clear();
        m_documentWidth->clear();
        m_form->setRowVisible(m_columns, false);
        return;
    }

    const QString width = QStringLiteral("%1 %2")
                              .arg(FromMm(model->widthMm, unit), 0, 'f', 2)
                              .arg(UnitLabel(unit));

    if (model->shape == ExportPrintShape::Roll)
    {
        m_sheetSize->setText(tr("Width: %1").arg(width));
    }
    else
    {
        m_sheetSize->setText(tr("Width: %1 x Height: %2")
                                 .arg(width)
                                 .arg(QStringLiteral("%1 %2")
                                          .arg(FromMm(model->heightMm, unit), 0, 'f', 2)
                                          .arg(UnitLabel(unit))));
    }

    // The column count is the user's, but a format that has none has exactly one — and it offers
    // no field at all: a count on a roll has one answer, and a field that can only say it is a
    // question the paper has already answered. The whole ROW goes, label included.
    m_form->setRowVisible(m_columns, model->hasColumns);
    if (!model->hasColumns)
    {
        // Back to one, so a document width is never computed from a count the paper cannot
        // hold — but the flag stays: a count she typed is still hers when she comes back to a
        // format that has one.
        m_columns->setValue(1);
    }
    else if (!m_columnsChosen)
    {
        m_columns->setValue(model->defaultColumns);
    }

    m_documentWidth->setText(QStringLiteral("%1 %2")
                                 .arg(FromMm(DocumentWidthMm(), unit), 0, 'f', 2)
                                 .arg(UnitLabel(unit)));
}
