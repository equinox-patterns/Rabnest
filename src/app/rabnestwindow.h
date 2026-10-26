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
#include "core/print/exporttemplate.h"
#include "core/print/printplan.h"

#include <QMainWindow>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QByteArray>
#include <QHash>
#include <QRectF>
#include <QStringList>
#include <QVector>

class ImportOverlay;
class PrintPanel;
class QDockWidget;
class QMenu;
class QTimer;
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

    /// The interface has changed language: every text of this window that is not rebuilt on use is
    /// set again here. Qt sends the event to every widget when a translator goes in or out, so each
    /// one answers for its own words — the window does not reach into its pages.
    void changeEvent(QEvent *event) override;

private:
    /// The menu bar, and the one menu it holds: `Languages`, the seventeen, and the one in force,
    /// ticked.
    ///
    /// **A menu of its own, not an item inside the application menu.** On macOS the menu that carries
    /// the application's name belongs to the system and holds what the system puts there — About,
    /// Services, Hide, Quit. Nothing else can live in it; the closest we can get is a menu of our own
    /// right beside it, which is one click away and does not pretend to be something it is not.
    void BuildMenu();
    void ChooseLanguage(const QString &code);

    /// « À propos » — what Rabnest is, its copyright, what it owes, and the versions under it.
    void ShowAbout();

    /// Sets again every text of this window that a change of language must move.
    void Retranslate();

    void Import();
    void GenerateOutput();
    void DeleteOutput(int index);

    /// The dock's download icon: this output's plan as an image — the same drawing the canvas
    /// shows, written as SVG or PNG at the size the user gives. A print run is answered by
    /// @ref ExportPrintOutput, since a run is not laid out to be pictured.
    void ExportOutputToImage(int index);

    /// The dock's download icon on a PRINT RUN: the run delivered as a PDF, which is the only file
    /// a print can be. One layer per size, one page per printed sheet, the paper's own marks under
    /// them and the cover page over them.
    void ExportPrintOutput(int index);

    /// The dock's printer icon: this output's plan on ONE sheet, shrunk to fit the paper the user
    /// picks, with the line that says which size it is, on what cloth, and at what scale.
    void PrintCutPlan(int index);

    /// The dock's printer icon on a PRINT RUN: the run on paper, one page per sheet, at 100 %.
    ///
    /// **The paper is not a choice, it is a consequence.** The sheets are already at their final
    /// size and they are glued to each other, so only a paper that holds the run's sheet whole is
    /// offered — and when the printer ends on another one, nothing is printed.
    void PrintPrintOutput(int index);
    void ShowOutputInfo(int index);
    void BackHome();

    /// Throws the PDF away — plans, rows, drawing, and the document the next output would have been
    /// generated from. It asks nothing: the caller decides whether there is something to lose.
    void CloseDocument();
    void OpenOverlay(bool generateFromDocument);

    /// The dock's printer icon: the print panel comes up, and the canvas shows the print run of
    /// that output — the sheets, their paper, the decor and the cover page.
    /// The Print tab generated: lay the run out, add its row, and show it.
    void GeneratePrint(const QStringList &sizeIds, const QString &modelToken, int columns,
                       double marginMm, double gapMm, bool plainOutline);

    /// One print run, kept whole: what the canvas needs to draw it again when its row is selected.
    struct PrintOutput
    {
        PrintPlan plan;
        ExportTemplate decor;
        QByteArray coverSvg;
        QRectF coverPaper;
        QRectF coverPage;
        bool framed = false;

        /// The PDF this run was laid from.
        ///
        /// Kept rather than read off the window's current document, because the window's document is
        /// the LAST import and a run can outlive it: a PDF exported from an older row must carry the
        /// payload it was actually laid from, not the one that happens to be open.
        QString sourcePath;
    };

    /// The print runs, by the row that presents them. A row is a plan or a print run, never both,
    /// and the map is what says which.
    QHash<int, PrintOutput> m_printOutputs;

    /// L'onglet Projecteur a généré : la recherche de largeur, sa ligne, et son canevas.
    void GenerateProjector(const QStringList &sizeIds, double marginCm, double gapCm);

    /// Une projection, gardée pour être écrite : son plan, et le PDF dont elle vient.
    struct ProjectorOutput
    {
        PrintPlan plan;

        /// Le PDF d'où la projection a été tirée — c'est lui qui porte la charge utile à réécrire,
        /// et il survit à l'import qui l'a lu.
        QString sourcePath;
    };

    /// Les projections, par la ligne qui les présente.
    QHash<int, ProjectorOutput> m_projectorOutputs;

    /// Le PDF de la projection : une page, ses trois calques sous les pièces, et sa charge utile.
    void ExportProjectorOutput(int index);

    /// Re-lays the print run with what the panel says, and redraws the canvas. Called on every
    /// change, because a print setting IS a nesting setting: the column count makes the document's
    /// width, and the margin and the gap go straight into the engine.
    void RebuildPrint(int index);

    WorkspacePage *m_workspace = nullptr;
    RunsDock *m_runs = nullptr;
    ImportOverlay *m_overlay = nullptr;

    /// The menu that lists the languages. Kept because its title is one of the words that has to
    /// follow a change of language.
    QMenu *m_languageMenu = nullptr;

    /// What the last accepted journey settled: the pattern, and what she kept of it. Empty until a
    /// first import — which is what closes « lay out another plan ».
    ImportedDocument m_document;

    /// The folder the last image was written to, proposed the next time. Empty until one is written.
    QString m_exportDirectory;
    QStringList m_sizeIds;
    QStringList m_pieceIds;

    /// Every plan on screen, in the dock's order — the dock says which row, this says which plan.
    QVector<CutPlan> m_plans;

    /// The LINING plan of a row, when the run asked for a pair — keyed by the same row.
    ///
    /// **A separate map, and keyed by row, for the same reason the print outputs are:** the fabric's
    /// plan keeps the shape it has always had, so everything that already reads one plan — the export,
    /// the image, the information sheet — goes on reading the fabric, and only the canvas and the
    /// figure line learn that there is a second cloth.
    QHash<int, CutPlan> m_liningPlans;

    /// The print panel, and the output it is working on.
    QDockWidget *m_printDock = nullptr;
    PrintPanel *m_printPanel = nullptr;
    int m_printIndex = -1;

    /// A press on a spin box is not one change but many: the print run is re-laid once the hand
    /// stops moving, not once per tick.
    QTimer *m_printTimer = nullptr;

    /// The print run is minutes on a real pattern, so it is laid out off the interface — the same
    /// worker pool the plan generation uses. A watcher is replaced on every change, and its result
    /// is dropped when the panel has moved on.
    QFutureWatcher<PrintPlan> *m_printWatch = nullptr;
    QThreadPool *m_printPool = nullptr;
};

#endif  // RABNESTWINDOW_H
