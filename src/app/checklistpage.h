/************************************************************************
 **
 **  @file   checklistpage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  One step of the journey: a list of things to tick, and nothing else.
 **
 **  @copyright
 **
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

#ifndef CHECKLISTPAGE_H
#define CHECKLISTPAGE_H

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// A step that asks one question: which of these do you keep?
///
/// Sizes and pieces are the same question twice, so they are the same page twice — one class, and
/// the difference is the words and the list. Duplicating it would mean two places to fix the day a
/// click has to behave differently, which is the day the two answers drift apart.
///
/// The page owns no model. It shows a list, it says what is ticked, and it says when the user is
/// done: the window decides what that means, because only the window knows which step it is.
class ChecklistPage : public QWidget
{
    Q_OBJECT

public:
    /// @param title    the step's own word — "Sizes", "Pieces"
    /// @param subtitle the one sentence that says what ticking does
    explicit ChecklistPage(const QString &title, const QString &subtitle, QWidget *parent = nullptr);

    /// Replaces the list. Every entry starts ticked: the common case is keeping all of them, and
    /// asking the user to tick eight sizes to say "tout" is a step they did not need.
    void SetItems(const QVector<QPair<QString, QString>> &items);  // (id, label)

    /// The ids of what is ticked, in list order.
    QStringList CheckedIds() const;

    /// Whether the user may go on: at least one tick. Ticking nothing is the one answer that has
    /// no meaning — there is no plan with no size — so the way forward is closed rather than
    /// leading to an empty canvas.
    void SetCanContinue(bool canContinue);
    void SetContinueText(const QString &text);
    void SetBackVisible(bool visible);

    /// The two words of the step, given again — the page is asked "sizes" or "pieces" by whoever
    /// builds it, so it cannot translate its own title: only the caller knows which question it is.
    void SetTexts(const QString &title, const QString &subtitle);

signals:
    void Continue();
    void Back();

protected:
    /// The interface has changed language: the list entries are the caller's (they come from the
    /// PDF), and only the page's own three words are set again here.
    void changeEvent(QEvent *event) override;

private:
    void Retranslate();

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_empty = nullptr;
    QWidget *m_entries = nullptr;
    QVBoxLayout *m_entriesLayout = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_continue = nullptr;
    QVector<QPair<QString, QCheckBox *>> m_boxes;  // id -> box, in list order

    void UpdateContinue();
};

#endif  // CHECKLISTPAGE_H
