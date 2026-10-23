/************************************************************************
 **
 **  @file   outputinfodialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  What one output was asked for, and what it cost.
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

#ifndef OUTPUTINFODIALOG_H
#define OUTPUTINFODIALOG_H

#include "core/cutplan.h"

#include <QDialog>

/// The two questions a line of the dock cannot answer: what was this output ASKED for, and what
/// did it cost.
///
/// **Read-only, and only facts.** Every figure comes from the plan itself — the cloth, the two
/// spacings, the fold and the direction are copied there when the plan is made, precisely so that
/// this dialogue can be answered months later without the request that produced it. The result half
/// says what the run produced: how many pieces, how much cloth, and how long it took.
///
/// It is a dialogue and not a window of its own: like the import journey, it belongs to the window
/// it is opened from.
class OutputInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OutputInfoDialog(const CutPlan &plan, QWidget *parent = nullptr);
};

#endif  // OUTPUTINFODIALOG_H
