/************************************************************************
 **
 **  @file   workspacepage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The canvas the cutting plan is drawn on.
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

#ifndef WORKSPACEPAGE_H
#define WORKSPACEPAGE_H

#include "core/cutplan.h"

#include <QVector>
#include <QWidget>

class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QListWidget;

/// The plan, drawn. One canvas, one list — one plan per size.
///
/// **Nothing leaves this page.** A cutting plan is what Ostara's own mode shows: pieces laid on a
/// length of fabric, the length written at the bottom. There is no file to write, so there is no
/// button to write one, and the canvas is the whole of the result.
class WorkspacePage : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspacePage(QWidget *parent = nullptr);

    /// Shows one plan, and nothing else: the list of them is the dock's, not the canvas's.
    void Show(const CutPlan &plan);

private:
    void Draw(const CutPlan &plan);

    QGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QLabel *m_detail = nullptr;
};

#endif  // WORKSPACEPAGE_H
