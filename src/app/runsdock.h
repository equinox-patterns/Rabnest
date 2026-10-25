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

    /// What an output is called: its size, and the kind of plan it is. Named once for the whole
    /// application — the dock shows it, and an exported file carries it.
    ///
    /// What a row IS — the three generations Rabnest knows how to make from one PDF.
    ///
    /// **Le genre n'est pas une étiquette : c'est ce que les icônes font.** Un plan part en image,
    /// un print en PDF à calques, une projection en PDF à une page : trois fichiers, trois gestes,
    /// et une ligne du dock ne peut pas les confondre.
    enum class Kind
    {
        Plan,
        Print,
        Projector
    };

    /// @param kind  what the row is. A print is not « cut » and does not say so: its name already
    ///              states what it covers — its sizes and its paper — and a kind tagged onto that is
    ///              a word nobody needs. A cutting plan keeps the tag, because two plans of one size
    ///              are two answers and only the kind tells them apart.
    static auto Name(const CutPlan &plan, Kind kind = Kind::Plan) -> QString;

    /// Adds a plan and selects it — the selection is what puts it on the canvas.
    ///
    /// @param kind  the row looks the same whatever it is, but what its icons DO is not.
    void AddRun(const CutPlan &plan, Kind kind = Kind::Plan);

    /// The row the user is on, or -1 when none is — what the canvas is showing.
    auto CurrentRow() const -> int;

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

    /// The interface has changed language: every word of the dock is set again — its title, its two
    /// buttons, and every row, because a row's name and its four tooltips were written once when the
    /// row appeared and would otherwise stay in the language it appeared in.
    void changeEvent(QEvent *event) override;

private:
    void BuildUi();
    void UpdateGlyphs();
    void RefreshEnabledState();

    /// Sets again the words of one row from the plan it presents.
    ///
    /// One place, called twice: when the row appears, and when the language changes. Two paths that
    /// each spelled the tooltips out would drift, and the drift would show as one row in French
    /// beside another in English.
    void ApplyRowTexts(int row);

    void Retranslate();

    QTableWidget *m_table = nullptr;

    /// What each row presents. Kept here because a row's name is not a fact about the table: it is
    /// what the plan is called in the language in force, and naming it again needs the plan.
    ///
    /// ⚠️ **A copy, and only for the words.** The plans themselves belong to the window; this is the
    /// least the dock needs to be able to re-say its own rows.
    struct Row
    {
        CutPlan plan;
        Kind kind = Kind::Plan;
    };

    QVector<Row> m_rows;

    /// The row announced last: a cell change inside it is not a change of output.

    int m_currentRow = -1;
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
