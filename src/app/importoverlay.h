/************************************************************************
 **
 **  @file   importoverlay.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The journey from a PDF to a cutting plan, as a panel INSIDE
 **          the window.
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

#ifndef IMPORTOVERLAY_H
#define IMPORTOVERLAY_H

#include "core/cutplan.h"
#include "core/importeddocument.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class BusyDialog;
class ChecklistPage;
class CutPlanPage;
class ImportPage;
class LayersPage;
class QFrame;
class QPushButton;
class QStackedWidget;

/// The steps, laid OVER the workspace — dimmed behind, one window on screen.
///
/// **Not a dialogue: a dialogue is a window of its own.** On macOS a modal QDialog brings its own
/// frame, and the eye reads two applications side by side — which is exactly what somebody who asked
/// for « a dialogue inside the interface » does not want. So this is a widget in the window, painted
/// over a scrim, with the steps in a card at its centre.
///
/// The journey does the laying out itself and only closes on success: a panel that vanished on a
/// failure would leave her with a journey to start over.
class ImportOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ImportOverlay(QWidget *parent);

    /// What the journey settled, so the same pattern can be laid out again on another cloth
    /// without being imported twice.
    const ImportedDocument &document() const { return m_document; }
    const QStringList &sizeIds() const { return m_sizeIds; }
    const QStringList &pieceIds() const { return m_pieceIds; }

    /// Opens on the FIRST step, for a pattern that is already imported: she wants another output
    /// of the same PDF — another size, another set of pieces — and the three steps are the same
    /// three steps, with nothing carried over but the document itself.
    ///
    /// ⚠️ **Not on the cloth step.** Skipping to the third step was the old « lay out another
    /// plan », and it could not work: the pieces page had never been filled, so the generation ran
    /// with an empty list of pieces and refused every size. Asking the three questions again is
    /// both the honest journey and the one that cannot lose a selection it never asked for.
    void StartAtSizes(const ImportedDocument &document);

signals:
    /// Empty when the journey was given up — closing the panel, or Escape.
    void Finished(const QVector<CutPlan> &plans);

    /// The Print tab asked to generate: the sizes it ticked, and the paper it chose. The window
    /// lays the run out, because the print needs the document and the settings of the panel it
    /// keeps — not the ones this page would have to rebuild.
    void PrintRequested(const QStringList &sizeIds, const QString &modelToken, int columns,
                        double marginMm, double gapMm, bool plainOutline);


protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void ChooseFile(const QString &path);
    void ShowSizes();
    void ShowPieces();
    void ShowCutPlan();
    void Generate();
    void ReduceLayers(const QString &target);
    void ShowPage(QWidget *page);
    void Finish(const QVector<CutPlan> &plans);

    BusyDialog *m_busy = nullptr;

    ImportedDocument m_document;
    QStringList m_sizeIds;
    QStringList m_pieceIds;

    QFrame *m_card = nullptr;
    QPushButton *m_cancel = nullptr;
    QStackedWidget *m_stack = nullptr;
    ImportPage *m_import = nullptr;
    ChecklistPage *m_sizes = nullptr;
    ChecklistPage *m_pieces = nullptr;
    CutPlanPage *m_cutPlan = nullptr;
    LayersPage *m_layers = nullptr;
};

#endif  // IMPORTOVERLAY_H
