/************************************************************************
 **
 **  @file   printrundialog.cpp
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

#include "app/printrundialog.h"

#include "app/factorywidgets.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

//---------------------------------------------------------------------------------------------------------------------
PrintRunDialog::PrintRunDialog(const QString &name, const QSizeF &sheetMm, int sheets,
                               const QVector<ExportPaper::Paper> &papers,
                               QPageSize::PageSizeId preferred, QWidget *parent)
    : QDialog(parent), m_papers(papers)
{
    setWindowTitle(tr("Print"));
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // ⚠️ **`%n`, et pas `%1`.** Un nombre glissé par `%1` laisse Qt devant une chaîne ordinaire, avec
    // UNE case de traduction : le tchèque, le polonais, le croate et le roumain changent la forme du
    // nom selon le nombre, et une seule case les oblige à choisir une forme fausse pour une partie
    // des nombres (« 2 listů » au lieu de « 2 listy »). `%n` fait de la chaîne un message numéral :
    // chaque langue reçoit autant de formes qu'elle en a besoin, et c'est Qt qui choisit à l'affichage.
    const QString count = tr("%n sheet(s)", nullptr, sheets);

    form->addRow(tr("Run"), new QLabel(name, this));
    form->addRow(tr("Sheets"),
                 new QLabel(tr("%1 of %2 × %3 mm")
                                .arg(count)
                                .arg(sheetMm.width(), 0, 'f', 1)
                                .arg(sheetMm.height(), 0, 'f', 1),
                            this));

    // ── Le papier ──────────────────────────────────────────────────────────────────────────────
    //
    // Offered when the sheet fits two papers, stated when it fits one. A one-entry list would be a
    // list that cannot be used, and the reader would look for the way out of it.
    if (papers.size() > 1)
    {
        m_choice = MakeComboBox(this);
        for (int index = 0; index < papers.size(); ++index)
        {
            m_choice->addItem(ExportPaper::Name(papers.at(index).id), index);
            if (papers.at(index).id == preferred)
            {
                m_choice->setCurrentIndex(index);
            }
        }
        form->addRow(tr("Paper"), m_choice);
    }
    else
    {
        form->addRow(tr("Paper"),
                     new QLabel(papers.isEmpty() ? QString() : ExportPaper::Name(papers.first().id),
                                this));
    }

    layout->addLayout(form);

    // **The scale is not asked because it is not a choice.** A sheet of a tiled run is glued to the
    // sheet beside it; printed at 95 % it is a plan whose seams do not meet, and nothing on the paper
    // says so. The note exists to say it before the printer's panel offers to change it.
    auto *note = new QLabel(tr("The sheets are already at their final size: they are printed at "
                               "100 %, one sheet per page."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);

    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    auto *printButton = buttons->addButton(tr("Print"), QDialogButtonBox::AcceptRole);
    printButton->setObjectName(QStringLiteral("primaryButton"));
    printButton->setCursor(Qt::PointingHandCursor);
    auto *cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setCursor(Qt::PointingHandCursor);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintRunDialog::paper() const -> QPageSize::PageSizeId
{
    if (m_papers.isEmpty())
    {
        return QPageSize::A4;
    }

    if (m_choice == nullptr)
    {
        return m_papers.first().id;
    }

    const int index = m_choice->currentData().toInt();
    return index >= 0 && index < m_papers.size() ? m_papers.at(index).id : m_papers.first().id;
}
