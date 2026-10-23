/************************************************************************
 **
 **  @file   rabnestwindow.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The window: the plans in a dock, the plan on the canvas.
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

#include "app/rabnestwindow.h"

#include "app/importoverlay.h"
#include "app/runsdock.h"
#include "app/workspacepage.h"

#include <QCoreApplication>
#include <QResizeEvent>
#include <QTimer>

namespace
{
auto Title() -> QString
{
    return QCoreApplication::applicationName().isEmpty()
               ? QStringLiteral("Rabnest")
               : QCoreApplication::applicationName();
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
RabnestWindow::RabnestWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(Title());
    resize(1240, 820);

    m_workspace = new WorkspacePage(this);
    setCentralWidget(m_workspace);

    m_runs = new RunsDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_runs);

    connect(m_runs, &RunsDock::Import, this, &RabnestWindow::Import);
    connect(m_runs, &RunsDock::NewPlan, this, &RabnestWindow::LayOutAgain);

    connect(m_runs, &RunsDock::CurrentChanged, this, [this](int index) {
        if (index >= 0 && index < m_plans.size())
        {
            m_workspace->Show(m_plans.at(index));
        }
    });

    // A window with nothing on it and no other way in would be a dead end, so the first gesture is
    // the only one: import a PDF. Asked AFTER the window is up, so the workspace is already there
    // underneath — the panel comes over it rather than standing in for it.
    QTimer::singleShot(0, this, [this] { OpenOverlay(false); });
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (m_overlay != nullptr)
    {
        m_overlay->setGeometry(rect());
        m_overlay->raise();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::Import()
{
    OpenOverlay(false);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::LayOutAgain()
{
    if (!m_document.isValid())
    {
        return;
    }

    OpenOverlay(true);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::OpenOverlay(bool planOnly)
{
    if (m_overlay != nullptr)
    {
        m_overlay->raise();
        m_overlay->setFocus();
        return;
    }

    m_overlay = new ImportOverlay(this);
    m_overlay->setGeometry(rect());

    if (planOnly)
    {
        m_overlay->StartAtCutPlan(m_document, m_sizeIds, m_pieceIds);
    }

    m_overlay->show();
    m_overlay->raise();
    m_overlay->setFocus();

    connect(m_overlay, &ImportOverlay::Finished, this, [this](const QVector<CutPlan> &plans) {
        // The pattern the journey settled is kept BEFORE the panel dies: laying the same pieces out
        // on another cloth is the next thing she may want, and it must not cost another import.
        if (!plans.isEmpty() && m_overlay->document().isValid())
        {
            m_document = m_overlay->document();
            m_sizeIds = m_overlay->sizeIds();
            m_pieceIds = m_overlay->pieceIds();
            m_runs->SetCanLayOutAgain(true);
        }

        m_overlay->deleteLater();
        m_overlay = nullptr;

        // The dock selects the row it just added, and the selection is what puts a plan on the
        // canvas: one path to the screen, so the list and the drawing cannot disagree.
        for (const CutPlan &plan : plans)
        {
            m_plans.append(plan);
            m_runs->AddRun(plan);
        }
    });
}
