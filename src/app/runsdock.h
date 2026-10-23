/************************************************************************
 **
 **  @file   runsdock.h
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

#ifndef RUNSDOCK_H
#define RUNSDOCK_H

#include "core/cutplan.h"

#include <QDockWidget>
#include <QVector>

class QListWidget;
class QPushButton;

/// The plans, as a dock — the same shape Ostara's export mode gives its outputs.
///
/// **An entry has to say what it is.** « Taille 42 » is a size, not a plan: the same size laid on
/// 140 cm of folded cloth and on 90 cm of flat cloth are two different answers to two different
/// questions, and a list that shows only the size makes them one line. Each entry carries its cloth,
/// its length and what it cost.
///
/// The two ways of getting another one are here, because they are different questions: laying the
/// SAME pattern out on another cloth — folded, flat, a narrower laize — and starting from another
/// PDF. Only the second one touches the import.
class RunsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit RunsDock(QWidget *parent = nullptr);

    /// Adds a plan and selects it.
    void AddRun(const CutPlan &plan);

    /// Whether a pattern is loaded, which is what makes « another plan » possible at all.
    void SetCanLayOutAgain(bool possible);

signals:
    void CurrentChanged(int index);
    void NewPlan();
    void Import();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_newPlan = nullptr;
    QVector<CutPlan> m_plans;
};

#endif  // RUNSDOCK_H
