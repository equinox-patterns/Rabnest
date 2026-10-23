/************************************************************************
 **
 **  @file   printplandialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Paper and orientation, and nothing else — the printer's panel asks the rest.
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

#include "app/printplandialog.h"
#include "app/factorywidgets.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>

//---------------------------------------------------------------------------------------------------------------------
PrintPlanDialog::PrintPlanDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Print the cutting plan"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_paper = MakeComboBox(this);
    m_paper->addItem(tr("A4"), static_cast<int>(Paper::A4));
    m_paper->addItem(tr("Letter"), static_cast<int>(Paper::Letter));
    form->addRow(tr("Paper"), m_paper);

    m_side = MakeComboBox(this);
    m_side->addItem(tr("Automatic"), static_cast<int>(Side::Auto));
    m_side->addItem(tr("Portrait"), static_cast<int>(Side::Portrait));
    m_side->addItem(tr("Landscape"), static_cast<int>(Side::Landscape));
    form->addRow(tr("Orientation"), m_side);

    layout->addLayout(form);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    auto *print = buttons->addButton(tr("Print"), QDialogButtonBox::AcceptRole);
    print->setObjectName(QStringLiteral("primaryButton"));
    print->setCursor(Qt::PointingHandCursor);
    auto *cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setCursor(Qt::PointingHandCursor);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPlanDialog::paper() const -> Paper
{
    return static_cast<Paper>(m_paper->currentData().toInt());
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPlanDialog::orientation(const QSizeF &planPx) const -> QPageLayout::Orientation
{
    switch (static_cast<Side>(m_side->currentData().toInt()))
    {
        case Side::Portrait:
            return QPageLayout::Portrait;
        case Side::Landscape:
            return QPageLayout::Landscape;
        default:
            break;
    }

    // Automatic: the plan's long side runs with the sheet's. On a plan 1600 mm wide and 1058 mm
    // tall, A4 upright gives 1:8,4 and A4 on its side 1:5,8 — a third more drawing for the same
    // sheet, for no decision the user should have to make.
    return planPx.width() > planPx.height() ? QPageLayout::Landscape : QPageLayout::Portrait;
}
