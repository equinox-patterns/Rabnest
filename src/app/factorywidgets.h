/************************************************************************
 **
 **  @file   factorywidgets.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  A combo box that actually honours the stylesheet, made in one place.
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

#ifndef FACTORYWIDGETS_H
#define FACTORYWIDGETS_H

#include <QComboBox>
#include <QListView>

/// Applies the Qt-styled dropdown to an ALREADY existing combo box — one built by `setupUi`, which no
/// factory can reach.
///
/// ⚠️ **The native macOS popup ignores the stylesheet entirely**: hover, selection colours, borders,
/// everything. A `QListView` as the combo's view makes the dropdown Qt-drawn, and the global sheet
/// applies to it like any other. The icon size is copied across because a dropdown whose icons jump
/// from 12 to 24 pixels between closed and open reads as two different widgets.
inline void ApplyComboBoxView(QComboBox *combo)
{
    auto *view = new QListView();
    combo->setView(view);
    view->setIconSize(combo->iconSize());
}

/// A combo box whose dropdown honours the stylesheet — **the only way one is created here.**
///
/// Same rule as Ostara's (`FactoryWidgets.h`, and `Modifications/Ostara/rules.md` §3.4): a
/// `new QComboBox` at a call site is a combo whose dropdown will be native, so the rule is a factory
/// rather than a habit.
inline auto MakeComboBox(QWidget *parent = nullptr) -> QComboBox *
{
    auto *combo = new QComboBox(parent);
    ApplyComboBoxView(combo);
    return combo;
}

#endif  // FACTORYWIDGETS_H
