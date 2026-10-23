/************************************************************************
 **
 **  @file   runsdock.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The dock of plans — what has been laid out, and the way to
 **          lay out another.
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

#include "app/runsdock.h"

#include <QListWidget>
#include <QObject>
#include <QString>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/// What an entry says: the size, and the cloth it was laid on. Nothing else.
///
/// A list is read at a glance, and a line that has to be read twice is a line that is not read at
/// all. The length, the piece count and the time belong to the plan being shown, not to every row
/// of a list where they would be the same words over and over.
auto EntryText(const CutPlan &plan) -> QString
{
    return QObject::tr("%1 — Fabric width %2 cm").arg(plan.sizeLabel).arg(plan.fabricWidthCm, 0, 'f', 1);
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
RunsDock::RunsDock(QWidget *parent)
    : QDockWidget(tr("Plans"), parent)
{
    setObjectName(QStringLiteral("runsDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(0);

    m_list = new QListWidget(body);
    m_list->setObjectName(QStringLiteral("outputsList"));
    m_list->setWordWrap(false);
    m_list->setUniformItemSizes(false);
    layout->addWidget(m_list, 1);

    m_newPlan = new QPushButton(tr("Lay out another plan"), body);
    m_newPlan->setObjectName(QStringLiteral("primaryButton"));
    m_newPlan->setCursor(Qt::PointingHandCursor);
    m_newPlan->setEnabled(false);
    layout->addWidget(m_newPlan);

    auto *import = new QPushButton(tr("Import a PDF"), body);
    import->setObjectName(QStringLiteral("secondaryButton"));
    import->setCursor(Qt::PointingHandCursor);
    layout->addWidget(import);

    setWidget(body);

    connect(m_list, &QListWidget::currentRowChanged, this, &RunsDock::CurrentChanged);
    connect(m_newPlan, &QPushButton::clicked, this, &RunsDock::NewPlan);
    connect(import, &QPushButton::clicked, this, &RunsDock::Import);
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::AddRun(const CutPlan &plan)
{
    m_plans.append(plan);

    auto *item = new QListWidgetItem(EntryText(plan), m_list);
    if (!plan.ok)
    {
        item->setToolTip(plan.error);
    }

    m_list->setCurrentRow(m_list->count() - 1);
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::SetCanLayOutAgain(bool possible)
{
    m_newPlan->setEnabled(possible);
}
