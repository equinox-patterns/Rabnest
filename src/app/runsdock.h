/************************************************************************
 **
 **  @file   runsdock.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The dock of outputs — what has been laid out, what each of
 **          them was asked for, and the way to make another one.
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
#include <QString>
#include <QVector>

class QPushButton;
class QTableWidget;

/// The outputs, as a dock — the same shape Ostara's export mode gives its outputs.
///
/// **One line, one output, and what it was asked for.** A line says what the output IS
/// (« Taille 46 - Cut ») and carries the four gestures a line needs: what it was asked for, export
/// it, print it, delete it. Two of the four are not wired yet and say so; the two that are wired do
/// their work here rather than in the window, so a row's identity never depends on an index kept
/// somewhere else.
///
/// **The dock never touches a plan.** It shows rows, and every gesture leaves as a signal carrying
/// the row it came from — the window owns the plans, and the canvas follows the selection.
class RunsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit RunsDock(QWidget *parent = nullptr);

    /// Adds a plan and selects it — the selection is what puts it on the canvas.
    void AddRun(const CutPlan &plan);

    /// Removes one row. The plans themselves are the window's; it removes its own copy first, so
    /// that the selection change this raises already names a valid row.
    void RemoveRun(int index);

    /// Empties the dock — « back to home » throws the whole PDF away.
    void Clear();

    /// Whether a pattern is loaded, which is what makes another output possible at all.
    void SetCanGenerate(bool possible);

signals:
    void CurrentChanged(int index);

    void GenerateRequested();
    void ExportSeveralRequested();

    void InfoRequested(int index);
    void ExportRequested(int index);
    void PrintRequested(int index);
    void DeleteRequested(int index);

    void HomeRequested();

protected:
    /// The hover rule: a button of the dock has an accent background under the pointer, so its icon
    /// must become the dark one for as long as it is there.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void BuildUi();
    void UpdateGlyphs();
    void RefreshEnabledState();

    QTableWidget *m_table = nullptr;
    QPushButton *m_generate = nullptr;
    QPushButton *m_exportSeveral = nullptr;
    QPushButton *m_home = nullptr;

    /// The buttons, with their icon name, so the hover rule can find the file again.
    struct Glyph
    {
        QPushButton *button = nullptr;
        QString iconName;
    };

    QVector<Glyph> m_glyphs;
};

#endif  // RUNSDOCK_H
