/************************************************************************
 **
 **  @file   rabnestwindow.h
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

#ifndef RABNESTWINDOW_H
#define RABNESTWINDOW_H

#include "core/importeddocument.h"

#include "core/cutplan.h"

#include <QMainWindow>
#include <QStringList>
#include <QVector>

class ImportOverlay;
class RunsDock;
class WorkspacePage;

/// Rabnest's single window: a dock of plans on the left, the current plan on the canvas.
///
/// **The pattern stays loaded.** Laying the same pieces out on another cloth — folded instead of
/// flat, 90 cm instead of 140 — is a different question about the SAME pattern, so it must not cost
/// another import. The window keeps what came out of the dialog and asks only for the cloth next
/// time.
class RabnestWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RabnestWindow(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void Import();
    void GenerateOutput();
    void DeleteOutput(int index);
    void ShowOutputInfo(int index);
    void BackHome();
    void OpenOverlay(bool generateFromDocument);

    WorkspacePage *m_workspace = nullptr;
    RunsDock *m_runs = nullptr;
    ImportOverlay *m_overlay = nullptr;

    /// What the last accepted journey settled: the pattern, and what she kept of it. Empty until a
    /// first import — which is what closes « lay out another plan ».
    ImportedDocument m_document;
    QStringList m_sizeIds;
    QStringList m_pieceIds;

    /// Every plan on screen, in the dock's order — the dock says which row, this says which plan.
    QVector<CutPlan> m_plans;
};

#endif  // RABNESTWINDOW_H
