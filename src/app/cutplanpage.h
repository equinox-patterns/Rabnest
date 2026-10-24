/************************************************************************
 **
 **  @file   cutplanpage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Which sizes to lay out, and the cloth they will be laid on.
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

#ifndef CUTPLANPAGE_H
#define CUTPLANPAGE_H

#include "core/cutplanrequest.h"

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QFormLayout;
class QTabWidget;
class PrintPanel;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// Which sizes get a plan, and the cloth they are laid on.
///
/// **A cutting plan is laid for ONE size** — that is what a plan is, and Ostara's own tab says so.
/// Asking for three gives three plans, not one plan over three sizes. So the sizes are ticked here
/// and not assumed: a size kept in the first step is not necessarily a size she wants to lay out.
class CutPlanPage : public QWidget
{
    Q_OBJECT

public:
    explicit CutPlanPage(QWidget *parent = nullptr);

    /// The sizes the first step kept, as (id, label). Every one starts ticked — the common case is
    /// laying out all of them, and asking her to tick four sizes to say « all » is a step nobody
    /// needed.
    void SetSizes(const QVector<QPair<QString, QString>> &sizes);

    /// The ids of the sizes to lay out, in list order.
    QStringList CheckedSizeIds() const;

    /// What the page currently says about the cloth.
    CutPlanRequest Request() const;

    /// Says why a plan could not be laid out, under the list.
    void ShowError(const QString &message);
    void ClearError();

    /// Which generation the button will make: the cutting plan, the print, or — one day — the
    /// plotter. The tabs are the user's choice, and the button above them obeys it.
    enum class Tab
    {
        Cutting,
        Print,
        Plotter
    };

    auto ActiveTab() const -> Tab;

    /// The print settings of this run, when the Print tab is the one being generated.
    auto PrintSettings() const -> PrintPanel *;

    /// Whether this run also lays out the **lining** — the second plan of a pair.
    ///
    /// A lined garment is cut twice: once in the fabric, once in the lining. The run makes both
    /// plans, and nothing separates them but the piece set and the width — the fold, the direction,
    /// the margin and the gap are the run's own and belong to both. So the page asks for ONE thing
    /// more, the lining's width, and only when the pattern has a piece cut in the lining.
    auto IsLining() const -> bool;

    /// The lining's own width, in centimetres. **It is the fabric's until she gives it one**: a
    /// lining is bought on its own bolt, and until that bolt is named it is the same one.
    auto LiningWidthCm() const -> double;

    /// Says whether a lining plan is at all possible — the pattern has a piece cut in the lining.
    ///
    /// The box is ticked the moment it becomes possible, and only then: a box that re-ticked itself
    /// on every visit would put back a cross she has just taken off. When it cannot be, it is off
    /// and disabled — offering a lining plan for a garment that has none is a question with no
    /// answer in it.
    void SetLiningAvailable(bool available);

signals:
    void Continue();
    void Back();

private:
    void UpdateContinue();
    void UpdateGenerateText();
    void UpdateLiningRow();

    QVector<QPair<QString, QCheckBox *>> m_boxes;  // id -> box, in list order
    QVBoxLayout *m_boxesLayout = nullptr;
    QWidget *m_boxesBlock = nullptr;
    QFormLayout *m_form = nullptr;
    QLabel *m_error = nullptr;
    QDoubleSpinBox *m_width = nullptr;
    QDoubleSpinBox *m_margin = nullptr;
    QDoubleSpinBox *m_gap = nullptr;
    QCheckBox *m_folded = nullptr;
    QCheckBox *m_oneWay = nullptr;
    QCheckBox *m_lining = nullptr;
    QDoubleSpinBox *m_liningWidth = nullptr;
    QPushButton *m_continue = nullptr;

    /// Whether the lining width was ever given a value of her own. Until it is, the field follows
    /// the fabric's, so that ticking « Lining » on the same bolt asks nothing twice.
    bool m_liningEdited = false;

    /// Whether a lining plan was possible the last time the page looked. The box is ticked when it
    /// BECOMES possible, not while it is — see SetLiningAvailable.
    bool m_liningWasAvailable = false;

    QTabWidget *m_tabs = nullptr;
    PrintPanel *m_print = nullptr;
};

#endif  // CUTPLANPAGE_H
