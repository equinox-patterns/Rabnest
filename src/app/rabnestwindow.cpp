/************************************************************************
 **
 **  @file   rabnestwindow.cpp
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

#include "app/rabnestwindow.h"

#include "core/projector.h"

#include "app/busydialog.h"
#include "app/importoverlay.h"
#include "app/exportimagedialog.h"
#include "app/aboutdialog.h"
#include "app/exportpdfdialog.h"
#include "app/outputinfodialog.h"
#include "app/printplandialog.h"
#include "app/printrundialog.h"
#include "app/rabnestlanguage.h"
#include "app/runsdock.h"
#include "app/workspacepage.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QPageLayout>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>
#include "app/print/printpanel.h"
#include "core/export/planimage.h"
#include "core/export/planimagewriter.h"
#include "core/export/plansheet.h"
#include "core/print/exportcoverpage.h"
#include "core/print/exportpaper.h"
#include "core/print/printmetrics.h"
#include "core/print/printpdf.h"
#include "core/print/printrecipe.h"
#include "ui/GroupOverlapDialog.h"

#include <QDockWidget>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QResizeEvent>
#include <QTimer>

namespace
{
auto Title() -> QString
{
    return QCoreApplication::applicationName().isEmpty()
               ? QStringLiteral("Rabnest")
               : QCoreApplication::applicationName();
}

/// What a print run covers, in words: its sizes.
///
/// **A range when the sizes are a range.** Three sizes or more that stand next to each other in the
/// drawing's layer list are one stretch of a graded pattern, and naming both ends is what says so —
/// « Taille 32 to Taille 36 » instead of the three names strung out. It is the LAYER LIST that
/// decides, never the order she ticked them in: « consecutive » is a fact about the drawing.
///
/// **Every name when they are not.** One size kept out and the two ends no longer say what was
/// printed; the list does, so the list it is. Two sizes are always a list — a range of two names is
/// a longer way of writing them.
auto SizesPhrase(const ImportedDocument &document, const QStringList &sizeIds) -> QString
{
    const QVector<ImportedLayer> &layers = document.layers();

    QStringList labels;
    QVector<int> positions;
    for (int index = 0; index < layers.size(); ++index)
    {
        if (sizeIds.contains(layers.at(index).id))
        {
            labels.append(layers.at(index).label);
            positions.append(index);
        }
    }

    if (labels.isEmpty())
    {
        return {};
    }

    if (labels.size() >= 3 && positions.size() == labels.size()
        && positions.last() - positions.first() + 1 == positions.size())
    {
        return RabnestWindow::tr("%1 to %2").arg(labels.first(), labels.last());
    }

    return labels.join(QStringLiteral(", "));
}

/// Whether the paper the printer ENDED on is one the run can be printed on.
///
/// The system panel is free to change the paper after we set it, and a driver may report a standard
/// paper under an identifier of its own — a custom one. So the answer is read from the size the
/// printer states, both ways round, with a millimetre of slack for the driver's own rounding: the
/// question is « does this sheet hold the run », not « what is this paper called ».
auto PaperHolds(const QPageLayout &layout, const QVector<ExportPaper::Paper> &papers) -> bool
{
    const QSizeF size = layout.fullRect(QPageLayout::Millimeter).size();
    const double width = std::min(size.width(), size.height());
    const double height = std::max(size.width(), size.height());

    for (const ExportPaper::Paper &paper : papers)
    {
        const double paperWidth = std::min(paper.sizeMm.width(), paper.sizeMm.height());
        const double paperHeight = std::max(paper.sizeMm.width(), paper.sizeMm.height());

        if (std::abs(width - paperWidth) <= 1.0 && std::abs(height - paperHeight) <= 1.0)
        {
            return true;
        }
    }

    return false;
}

/// The papers a run can be printed on, as one phrase: « A4 or Letter ».
auto PaperNames(const QVector<ExportPaper::Paper> &papers) -> QString
{
    QStringList names;
    names.reserve(papers.size());
    for (const ExportPaper::Paper &paper : papers)
    {
        names.append(ExportPaper::Name(paper.id));
    }

    if (names.size() <= 1)
    {
        return names.value(0);
    }

    const QString last = names.takeLast();
    return RabnestWindow::tr("%1 or %2").arg(names.join(QStringLiteral(", ")), last);
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
RabnestWindow::RabnestWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(Title());
    resize(1240, 820);

    BuildMenu();

    m_workspace = new WorkspacePage(this);
    setCentralWidget(m_workspace);

    m_runs = new RunsDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_runs);

    connect(m_runs, &RunsDock::HomeRequested, this, &RabnestWindow::BackHome);
    connect(m_runs, &RunsDock::GenerateRequested, this, &RabnestWindow::GenerateOutput);
    connect(m_runs, &RunsDock::InfoRequested, this, &RabnestWindow::ShowOutputInfo);
    connect(m_runs, &RunsDock::DeleteRequested, this, &RabnestWindow::DeleteOutput);
    connect(m_runs, &RunsDock::ExportRequested, this, &RabnestWindow::ExportOutputToImage);
    connect(m_runs, &RunsDock::PrintRequested, this, &RabnestWindow::PrintCutPlan);

    // ⚠️ **A run is edited on the canvas and kept in the dock's row, and those were two copies.**
    // The canvas drew its own; the row kept the run as it was laid out. A hand moved a piece, the row
    // never heard of it, and everything that reads the row — the export, the printer, the information
    // dialogue, and the selection that shows the row again — answered with the positions the nesting
    // had given, so the gesture was lost and the drawing jumped back. One move, one hand-over: the
    // row is the run from then on.
    connect(m_workspace, &WorkspacePage::PrintPlanEdited, this, [this] {
        const int row = m_runs->CurrentRow();
        if (row >= 0 && m_printOutputs.contains(row))
        {
            m_printOutputs[row].plan = m_workspace->PrintPlanShown();
        }

        // Une projection se déplace comme un print : même canevas, même geste, même retour.
        if (row >= 0 && m_projectorOutputs.contains(row))
        {
            m_projectorOutputs[row].plan = m_workspace->PrintPlanShown();
        }
    });

    connect(m_runs, &RunsDock::CurrentChanged, this, [this](int index) {
        if (index < 0)
        {
            return;
        }

        // A row is a plan or a print run: the map says which, and the drawing follows.
        if (m_printOutputs.contains(index))
        {
            const PrintOutput &print = m_printOutputs.value(index);
            m_workspace->ShowPrint(print.plan, print.decor, print.framed, print.coverSvg,
                                   print.coverPaper, print.coverPage);
            return;
        }

        // Une projection se dessine par le canevas du print : une page blanche, les pièces de
        // toutes les tailles dessus, et aucun repère — il n'y a pas de feuille à assembler.
        if (m_projectorOutputs.contains(index))
        {
            const ProjectorOutput &projection = m_projectorOutputs.value(index);
            m_workspace->ShowPrint(projection.plan, ExportTemplate(), false, QByteArray(),
                                   QRectF(), QRectF());
            return;
        }

        if (index < m_plans.size())
        {
            // The pair, when there is one: the fabric, and the lining laid beside it.
            m_workspace->Show(m_plans.at(index), m_liningPlans.contains(index) ? &m_liningPlans[index]
                                                                              : nullptr);
        }
    });

    // The zoom of the canvas. `QKeySequence::ZoomIn` is ⌘+ on this platform, and it accepts ⌘= as
    // well — the key a keyboard actually has, where the plus is a shifted equal.
    const auto addZoom = [this](const QKeySequence &sequence, void (WorkspacePage::*slot)()) {
        auto *action = new QAction(this);
        action->setShortcut(sequence);
        action->setShortcutContext(Qt::WindowShortcut);
        connect(action, &QAction::triggered, m_workspace, slot);
        addAction(action);
    };
    addZoom(QKeySequence(QKeySequence::ZoomIn), &WorkspacePage::ZoomIn);
    addZoom(QKeySequence(QKeySequence::ZoomOut), &WorkspacePage::ZoomOut);

    // A window with nothing on it and no other way in would be a dead end, so the first gesture is
    // the only one: import a PDF. Asked AFTER the window is up, so the workspace is already there
    // underneath — the panel comes over it rather than standing in for it.
    QTimer::singleShot(0, this, [this] { OpenOverlay(false); });
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::BuildMenu()
{
    m_languageMenu = menuBar()->addMenu(tr("Languages"));

    // One and only one language at a time: an action group makes the menu tell the truth about which
    // one is in force, rather than each action guessing it.
    auto *group = new QActionGroup(this);
    group->setExclusive(true);

    const QString inForce = RabnestLanguage::Current();
    for (const RabnestLanguage::Language &language : RabnestLanguage::All())
    {
        QAction *action = m_languageMenu->addAction(language.label);
        action->setCheckable(true);
        action->setChecked(language.code == inForce);
        action->setData(language.code);
        group->addAction(action);

        const QString code = language.code;
        connect(action, &QAction::triggered, this, [this, code] { ChooseLanguage(code); });
    }

    // ── À propos ────────────────────────────────────────────────────────────────────────────────
    //
    // **Le rôle fait le travail, pas le menu.** Sur macOS le menu qui porte le nom de l'application
    // appartient au système : « About Rabnest » y est déjà, et Qt redirige ce clic vers l'action qui
    // porte `AboutRole` au lieu d'ouvrir le panneau standard. L'action voyage donc dans le menu
    // Langues pour être trouvée par Qt, qui l'en retire ; ailleurs, où il n'existe pas de menu
    // d'application, elle a un menu à elle.
    auto *about = new QAction(tr("About Rabnest"), this);
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &RabnestWindow::ShowAbout);

#ifdef Q_OS_MACOS
    m_languageMenu->addSeparator();
    m_languageMenu->addAction(about);
#else
    menuBar()->addMenu(tr("Help"))->addAction(about);
#endif
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ShowAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ChooseLanguage(const QString &code)
{
    if (code == RabnestLanguage::Current())
    {
        return;
    }

    // Stored first: if loading the file failed, the next launch still opens in the language she
    // asked for, and the failure is about one missing file, not about a lost choice.
    RabnestLanguage::Store(code);

    // Installing or removing the translator makes Qt send `LanguageChange` to every widget: the
    // window, the dock and the pages each set their own words again. Nothing is rebuilt, so nothing
    // of what she has laid out is lost.
    RabnestLanguage::Load(code);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
    }

    QMainWindow::changeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::Retranslate()
{
    // The application's name is not translated: it is a name, and a name does not change with the
    // language of the window it is written in.
    setWindowTitle(Title());

    if (m_languageMenu != nullptr)
    {
        m_languageMenu->setTitle(tr("Languages"));
    }

    // The pages and the dock answer for their own words: Qt hands them the same event, and each one
    // knows which of its texts a language change moves.
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (m_overlay != nullptr)
    {
        m_overlay->setGeometry(rect());
        m_overlay->raise();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::Import()
{
    OpenOverlay(false);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::GenerateOutput()
{
    if (!m_document.isValid())
    {
        return;
    }

    // The three steps again, on the same PDF. « Another output » is another answer to the same
    // three questions — another size, another set of pieces — and the journey asks them rather than
    // guessing which ones changed.
    OpenOverlay(true);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::DeleteOutput(int index)
{
    if (index < 0 || index >= m_plans.size())
    {
        return;
    }

    // **The window's list first.** Removing the row raises the selection change, and that change
    // reads this very vector: it has to name the surviving plans already, or the canvas would show
    // the plan one row off.
    m_plans.remove(index);
    m_printOutputs.remove(index);
    m_projectorOutputs.remove(index);
    m_liningPlans.remove(index);

    // The maps are keyed by ROW, so every key above the one removed moves down with its row.
    QHash<int, PrintOutput> reindexed;
    for (auto it = m_printOutputs.cbegin(); it != m_printOutputs.cend(); ++it)
    {
        reindexed.insert(it.key() > index ? it.key() - 1 : it.key(), it.value());
    }
    m_printOutputs = reindexed;

    QHash<int, CutPlan> liningLeft;
    for (auto it = m_liningPlans.cbegin(); it != m_liningPlans.cend(); ++it)
    {
        liningLeft.insert(it.key() > index ? it.key() - 1 : it.key(), it.value());
    }
    m_liningPlans = liningLeft;

    QHash<int, ProjectorOutput> projectionsLeft;
    for (auto it = m_projectorOutputs.cbegin(); it != m_projectorOutputs.cend(); ++it)
    {
        projectionsLeft.insert(it.key() > index ? it.key() - 1 : it.key(), it.value());
    }
    m_projectorOutputs = projectionsLeft;

    m_runs->RemoveRun(index);

    if (m_plans.isEmpty())
    {
        m_workspace->Clear();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::PrintCutPlan(int index)
{
    if (index < 0 || index >= m_plans.size())
    {
        return;
    }

    // ⚠️ **A print run is not a cutting plan.** Its row exists so the print panel has something to
    // present, and the plan beside it is a shell: printing it would put an empty sheet on paper. A
    // run has its own printing, which is its file's drawing and not a plan shrunk onto one sheet.
    if (m_printOutputs.contains(index))
    {
        PrintPrintOutput(index);
        return;
    }

    const CutPlan &plan = m_plans.at(index);
    if (!plan.ok)
    {
        return;
    }

    // The scene IS the page, so the plan printed has to be the plan on screen.
    //
    // ⚠️ **Une projection ne se redessine pas comme un plan de coupe** : sa ligne n'a pas de poses,
    // et `Show` viderait la scène. Son canevas est celui du print, et c'est lui qu'on remet.
    if (m_projectorOutputs.contains(index))
    {
        const ProjectorOutput &projection = m_projectorOutputs.value(index);
        m_workspace->ShowPrint(projection.plan, ExportTemplate(), false, QByteArray(), QRectF(),
                               QRectF());
    }
    else
    {
        m_workspace->Show(plan);
    }

    // ⚠️ **La feuille doit contenir ce qui est DESSINÉ** — cadres, titres et flèches compris —, pas le
    // seul tissu : le papier couperait les titres que l'écran montre.
    const QRectF shownRect = m_workspace->PlanRect();
    const QRectF drawnRect =
        shownRect.isNull() ? QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx) : shownRect;
    const QSizeF planPx = drawnRect.size();

    PrintPlanDialog choice(this);
    if (choice.exec() != QDialog::Accepted)
    {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(choice.paper() == PrintPlanDialog::Paper::A4 ? QPageSize::A4
                                                                               : QPageSize::Letter));
    printer.setPageOrientation(choice.orientation(planPx));

    // ⚠️ **`fullPage` false, so the painter starts at the corner the printer can actually reach.**
    // With it true, Qt reports the WHOLE sheet as paintable and the driver's own unprintable margin
    // is ignored — a plan laid out to the very edge and cropped by the machine. The half centimetre
    // of air below is on top of this, not instead of it.
    printer.setFullPage(false);

    QPrintDialog panel(&printer, this);
    panel.setWindowTitle(tr("Print the cutting plan"));
    if (panel.exec() != QDialog::Accepted)
    {
        return;
    }

    // ⚠️ **From here the page is what the printer RECEIVED, not what was asked for.** The system panel
    // lets the paper change, and laying the plan out on the sheet we picked would print on a sheet
    // that is not in the machine. `fullPage` is set again because the panel may have moved it.
    printer.setFullPage(false);
    const QRectF printable(printer.pageRect(QPrinter::DevicePixel));
    if (printable.isEmpty())
    {
        QMessageBox::warning(this, tr("Print"), tr("This printer reports no printable area."));
        return;
    }

    const double devicePerMm = printer.resolution() / 25.4;
    const double air = 5.0 * devicePerMm; // half a centimetre, on the four sides

    QFont font = m_workspace->font();
    font.setPointSizeF(9.0);

    // ⚠️ **Measured ON THE PRINTER, never on the screen.** A point is physical, but a metric is in the
    // device's pixels: the same nine-point line is fifteen pixels tall on a screen and two hundred on
    // a sheet at 1200 dpi. Measured on the screen, the band under the plan came out half a millimetre
    // tall and the line was drawn outside it.
    const double bandPx = QFontMetricsF(font, &printer).height() * 1.6;

    const PlanSheet::Sheet sheet = PlanSheet::Of(planPx, printable, air, bandPx);
    if (sheet.scale <= 0.0)
    {
        QMessageBox::warning(this, tr("Print"), tr("The plan does not fit on that page."));
        return;
    }

    QPainter painter;
    if (!painter.begin(&printer))
    {
        QMessageBox::warning(this, tr("Print"), tr("The printer could not be opened."));
        return;
    }

    // The thinnest stroke, in scene pixels, that still measures a tenth of a millimetre on paper:
    // a floor written in device pixels would print a line a quarter of that, and a plan drawn in
    // grey.
    const double floorPx = 0.1 * devicePerMm / sheet.scale;
    m_workspace->RenderPlan(&painter, drawnRect, sheet.planRect, floorPx);

    // ── La ligne sous le plan ────────────────────────────────────────────────────────────────────
    //
    // What the sheet is, and — its own figure, on the right — what it was shrunk to. A plan printed at
    // 1:6 that does not say so is a ruler that lies by six.
    const double widthMm = plan.fabricWidthCm * 10.0;
    const double lengthMm = plan.usedHeightPx * 25.4 / 96.0;
    const double ratio = (sheet.scale / devicePerMm) / (25.4 / 96.0);

    const QString caption =
        tr("%1 - Cutting plan: fabric width %2 / %3 inches, length %4 / %5 yards")
            .arg(plan.sizeLabel, tr("%1 cm").arg(widthMm / 10.0, 0, 'f', 1),
                 QString::number(widthMm / 25.4, 'f', 0),
                 tr("%1 cm").arg(lengthMm / 10.0, 0, 'f', 1),
                 QString::number(lengthMm / 25.4 / 36.0, 'f', 2));

    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(sheet.captionRect, Qt::AlignLeft | Qt::AlignVCenter, caption);
    painter.drawText(sheet.captionRect, Qt::AlignRight | Qt::AlignVCenter,
                     ratio > 0.0 ? tr("Scale 1:%1").arg(1.0 / ratio, 0, 'f', 1) : QString());

    painter.end();
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::PrintPrintOutput(int index)
{
    const PrintOutput &output = m_printOutputs.value(index);
    const CutPlan &row = m_plans.at(index);
    const PrintPlan &plan = output.plan;
    if (!plan.ok)
    {
        return;
    }

    // The sheet the run is laid out on: one cell of its tiling, or the bin itself when it is a roll.
    const QSizeF sheetPx = plan.grid.valid
                               ? QSizeF(plan.grid.cellWidthPx, plan.grid.cellHeightPx)
                               : QSizeF(plan.binWidthPx, plan.usedHeightPx);
    const QSizeF sheetMm(PrintMetrics::PxToMm(sheetPx.width()),
                         PrintMetrics::PxToMm(sheetPx.height()));

    // **The papers are deduced, not offered.** A format belongs to one pair of papers, and inside
    // that pair only the ones that hold its sheet whole: an A4 run on A4, a framed run on A4 or on
    // Letter. When none can carry it, the run is not printed at all — a sheet cut short does not
    // glue to the one beside it, and nothing on the paper would say so.
    const ExportPrintModel *model = FindExportPrintModel(plan.modelToken);
    if (model == nullptr)
    {
        return;
    }

    const QVector<ExportPaper::Paper> papers = ExportPaper::Fitting(model->family, sheetMm);
    if (papers.isEmpty())
    {
        QMessageBox::warning(this, tr("Print"),
                             tr("This run is %1 × %2 mm per sheet, and no sheet of paper holds it "
                                "whole. Export it as a PDF and print it from there.")
                                 .arg(sheetMm.width(), 0, 'f', 1)
                                 .arg(sheetMm.height(), 0, 'f', 1));
        return;
    }

    const int sheets = plan.Pages(!output.coverPaper.isEmpty());

    // **The paper the machine already has, when the run fits it.** A framed run fits two papers and
    // only one of them is probably loaded; proposing the other would print on a sheet of the wrong
    // size for anyone who presses Print without reading.
    QPageSize::PageSizeId preferred = papers.first().id;
    const QPageSize loaded = QPrinter(QPrinter::HighResolution).pageLayout().pageSize();
    for (const ExportPaper::Paper &paper : papers)
    {
        if (paper.id == loaded.id())
        {
            preferred = paper.id;
        }
    }

    PrintRunDialog choice(row.sizeLabel, sheetMm, sheets, papers, preferred, this);
    if (choice.exec() != QDialog::Accepted)
    {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(choice.paper()));
    printer.setPageOrientation(QPageLayout::Portrait);

    // ⚠️ **`fullPage` false**, as for a cutting plan: the page the painter is given is the paper the
    // machine can actually ink, and not the whole sheet it was told about.
    printer.setFullPage(false);

    QPrintDialog panel(&printer, this);
    panel.setWindowTitle(tr("Print the print run"));
    if (panel.exec() != QDialog::Accepted)
    {
        return;
    }

    printer.setFullPage(false);

    // ⚠️ **The system panel may have changed the paper, and that is the mistake this exists to
    // stop.** An A4 run printed on Letter still looks like a plan, and is a plan whose sheets are
    // two millimetres short. What the printer actually received is what is checked.
    if (!PaperHolds(printer.pageLayout(), papers))
    {
        QMessageBox::warning(
            this, tr("Print"),
            tr("These sheets are %1 × %2 mm, and this run prints on %3 only. Nothing was printed.")
                .arg(sheetMm.width(), 0, 'f', 1)
                .arg(sheetMm.height(), 0, 'f', 1)
                .arg(PaperNames(papers)));
        return;
    }

    // The run is drawn from the PDF it was read from, exactly as its export is: the paper is the
    // file's drawing, never a second drawing made for the printer.
    const ImportedDocument document = ImportedDocument::Read(output.sourcePath);
    if (!document.isValid() || document.kind() != ImportedDocument::Kind::Ostara)
    {
        QMessageBox::warning(
            this, tr("Print"),
            tr("The PDF this print was laid from can no longer be read:\n%1\n\n%2")
                .arg(output.sourcePath, document.error()));
        return;
    }

    PrintPdf::Job job;
    job.payload = document.payload();
    job.plan = output.plan;
    job.decor = output.decor;
    job.framed = output.framed;
    job.coverSvg = output.coverSvg;
    job.coverPaper = output.coverPaper;
    job.coverPage = output.coverPage;
    job.sourcePath = output.sourcePath;
    job.title = row.sizeLabel;

    QString error;
    if (!PrintPdf::Paint(printer, job, &error))
    {
        QMessageBox::warning(this, tr("Print"), error);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ExportOutputToImage(int index)
{
    if (index < 0 || index >= m_plans.size())
    {
        return;
    }

    // ⚠️ **A print run is not a cutting plan.** Its row exists so the print panel has something to
    // present; the plan beside it is a shell, and rendering it would write an empty sheet under the
    // name of a real output. A print is delivered as a PDF, which is the only file a print can be.
    if (m_projectorOutputs.contains(index))
    {
        ExportProjectorOutput(index);
        return;
    }

    if (m_printOutputs.contains(index))
    {
        ExportPrintOutput(index);
        return;
    }

    const CutPlan &plan = m_plans.at(index);
    if (!plan.ok)
    {
        return;
    }

    // ⚠️ **The scene IS the drawing**, so the plan being written has to be the plan on screen —
    // otherwise the file is the picture of whatever was left there. Showing it also puts the row's
    // output in front of her, which is where she expects to be after pressing its icon.
    m_workspace->Show(plan);

    // ⚠️ **Ce qui est rendu, c'est ce qui est DESSINÉ** — cadres, titres et flèches compris. Rendre le
    // seul rectangle du tissu les coupait en haut et en bas : le fichier ne contenait pas ce que
    // l'écran montrait. La marge de VUE, elle, ne rentre toujours pas : elle n'existe que dans la
    // fenêtre. Le métrage reste celui du tissu — deux choses différentes.
    const QRectF shownRect = m_workspace->PlanRect();
    const PlanImage::Plan image = PlanImage::Of(
        shownRect.isNull() ? QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx) : shownRect);

    ExportImageDialog dialog(image, RunsDock::Name(plan), m_exportDirectory, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString path = QDir(dialog.directory()).filePath(dialog.fileName());

    const bool written = PlanImageWriter::Write(
        path,
        [this](QPainter *painter, const QRectF &sourcePx, const QRectF &targetPx) {
            m_workspace->RenderPlan(painter, sourcePx, targetPx);
        },
        image.rectPx, dialog.target(), dialog.format(), RunsDock::Name(plan));

    if (!written)
    {
        QMessageBox::warning(this, tr("Export"),
                             tr("The file could not be written:\n%1").arg(path));
        return;
    }

    // Remembered only once a file exists: proposing a folder the export never reached would make
    // the next one fail in the same place.
    m_exportDirectory = dialog.directory();
    QMessageBox::information(this, tr("Export"), tr("File written:\n%1").arg(path));
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ExportPrintOutput(int index)
{
    const PrintOutput &output = m_printOutputs.value(index);
    const CutPlan &row = m_plans.at(index);

    // **The folder proposed is the one the run came from.** The PDF this print was read from has a
    // folder, and that is where its print belongs; a delivery that started in the Documents folder
    // would be a delivery she has to hunt for. The name proposed is the row's own, so the file she
    // is about to write is the one she pressed.
    ExportPdfDialog dialog(RunsDock::Name(row, RunsDock::Kind::Print),
                           QFileInfo(output.sourcePath).absolutePath(), this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString path = QDir(dialog.directory()).filePath(dialog.fileName());

    // The plan is drawn from the payload, and the payload is in the PDF it was read from. Read again
    // rather than kept: a print run outlives the import it came from, and the file is the only thing
    // that still knows what the run was drawn from.
    const ImportedDocument document = ImportedDocument::Read(output.sourcePath);
    if (!document.isValid() || document.kind() != ImportedDocument::Kind::Ostara)
    {
        QMessageBox::warning(
            this, tr("Export"),
            tr("The PDF this print was laid from can no longer be read:\n%1\n\n%2")
                .arg(output.sourcePath, document.error()));
        return;
    }

    PrintPdf::Job job;
    job.payload = document.payload();
    job.plan = output.plan;
    job.decor = output.decor;
    job.framed = output.framed;
    job.coverSvg = output.coverSvg;
    job.coverPaper = output.coverPaper;
    job.coverPage = output.coverPage;
    job.sourcePath = output.sourcePath;
    job.title = QFileInfo(path).fileName();

    QString error;
    if (!PrintPdf::Write(job, path, &error))
    {
        QMessageBox::warning(this, tr("Export"), error);
        return;
    }

    QMessageBox::information(this, tr("Export"), tr("File written:\n%1").arg(path));
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ShowOutputInfo(int index)
{
    if (index < 0 || index >= m_plans.size())
    {
        return;
    }

    // ⚠️ **A print run is answered with the PAPER's facts, not the cloth's.** Its row in the dock is
    // a shell — the plan that presents it carries no poses and no fabric — so the cutting plan's
    // dialogue answered every question with a zero: no page count, a fabric length the run never
    // had, and « Pieces 0 » for a run that lays nine of them.
    if (m_printOutputs.contains(index))
    {
        const PrintOutput &output = m_printOutputs.value(index);
        const PrintPlan &plan = output.plan;

        PrintInfo info;
        info.title = m_plans.at(index).sizeLabel;

        const ExportPrintModel *model = FindExportPrintModel(plan.modelToken);
        info.format = model != nullptr
                          ? QCoreApplication::translate("OExportPrintModel", model->label)
                          : tr("Unknown format");
        info.columns = plan.columns;
        info.hasColumns = model != nullptr && model->hasColumns;
        info.marginCm = plan.marginCm;
        info.gapCm = plan.gapCm;

        // The cover page is on a sheet of its own exactly when the run carries one — the rectangle
        // the canvas draws it in.
        info.coverPage = !output.coverPaper.isEmpty();
        info.pages = plan.Pages(info.coverPage);
        info.seconds = plan.seconds;
        info.sizes = plan.layers.size();

        for (const PrintLayer &layer : plan.layers)
        {
            for (const PrintPiece &piece : layer.pieces)
            {
                if (!info.pieces.contains(piece.name))
                {
                    info.pieces.append(piece.name);
                }
            }
        }

        OutputInfoDialog dialog(info, this);
        dialog.exec();
        return;
    }

    if (m_projectorOutputs.contains(index))
    {
        const ProjectorOutput &projection = m_projectorOutputs.value(index);

        PrintInfo info;
        info.title = RunsDock::Name(m_plans.at(index), RunsDock::Kind::Projector);
        // Une projection n'a pas de modèle de papier : ce qui la décrit est sa page.
        info.formatLabel = tr("Plan");
        info.format = tr("Projection, one page at 1:1");
        info.marginCm = projection.plan.marginCm;
        info.gapCm = projection.plan.gapCm;
        info.pages = 1;
        info.seconds = projection.plan.seconds;
        info.sizes = projection.plan.layers.size();

        for (const PrintLayer &layer : projection.plan.layers)
        {
            for (const PrintPiece &piece : layer.pieces)
            {
                if (!info.pieces.contains(piece.name))
                {
                    info.pieces.append(piece.name);
                }
            }
        }

        OutputInfoDialog dialog(info, this);
        dialog.exec();
        return;
    }

    OutputInfoDialog dialog(m_plans.at(index),
                            m_liningPlans.contains(index) ? &m_liningPlans[index] : nullptr, this);
    dialog.exec();
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::BackHome()
{
    // ⚠️ **The question is asked only when there IS something to lose.** `isValid()` is true of a
    // document nobody imported — it only says no error was recorded — so the window used to promise
    // the loss of plans, rows and a drawing that had never existed.
    const bool hasWork = !m_plans.isEmpty() || !m_document.path().isEmpty();

    if (hasWork)
    {
        // What is lost, named: the PDF she imported, not « the current file ».
        const QString file = QFileInfo(m_document.path()).fileName();

        QMessageBox question(QMessageBox::Question, tr("Back to home"),
                             tr("Every output of \"%1\" will be lost.\n\n"
                                "Make sure you have exported or printed everything you need.")
                                 .arg(file),
                             QMessageBox::Yes | QMessageBox::Cancel, this);
        question.setDefaultButton(QMessageBox::Cancel);
        question.button(QMessageBox::Yes)->setText(tr("Back to home"));

        if (question.exec() != QMessageBox::Yes)
        {
            return;
        }
    }

    CloseDocument();
    OpenOverlay(false);
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::CloseDocument()
{
    // Nothing of the previous PDF survives: not the plans, not the rows, not the drawing, not the
    // document the next output would have been generated from.
    m_plans.clear();
    m_liningPlans.clear();
    m_projectorOutputs.clear();
    m_runs->Clear();
    m_workspace->Clear();

    m_document = ImportedDocument();
    m_sizeIds.clear();
    m_pieceIds.clear();
    m_runs->SetCanGenerate(false);

    // The print panel belonged to a document that no longer exists.
    m_printIndex = -1;
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::OpenOverlay(bool generateFromDocument)
{
    if (m_overlay != nullptr)
    {
        m_overlay->raise();
        m_overlay->setFocus();
        return;
    }

    m_overlay = new ImportOverlay(this);
    m_overlay->setGeometry(rect());

    if (generateFromDocument)
    {
        m_overlay->StartAtSizes(m_document);
    }

    m_overlay->show();
    m_overlay->raise();
    m_overlay->setFocus();

    connect(m_overlay, &ImportOverlay::PrintRequested, this, &RabnestWindow::GeneratePrint);
    connect(m_overlay, &ImportOverlay::ProjectorRequested, this, &RabnestWindow::GenerateProjector);

    connect(m_overlay, &ImportOverlay::Finished, this,
            [this](const QVector<CutPlan> &plans, const QVector<CutPlan> &lining, bool reduced) {
        // The pattern the journey settled is kept BEFORE the panel dies: laying the same pieces out
        // on another cloth is the next thing she may want, and it must not cost another import.
        if (!plans.isEmpty() && m_overlay->document().isValid())
        {
            m_document = m_overlay->document();
            m_sizeIds = m_overlay->sizeIds();
            m_pieceIds = m_overlay->pieceIds();
            m_runs->SetCanGenerate(true);
        }

        m_overlay->deleteLater();
        m_overlay = nullptr;

        // ⚠️ **Un PDF qui n'est pas d'Ostara n'a rien à faire sur la fenêtre principale.** Le réduire
        // rend un fichier et rien d'autre : ni plan à dessiner, ni taille, ni pièce, donc ni sortie à
        // garder. Laisser la fenêtre là n'offrirait qu'une « Back to home » qui propose de perdre ce
        // qui n'existe pas. L'application revient donc à « import a PDF », sans question.
        if (reduced)
        {
            CloseDocument();
            QTimer::singleShot(0, this, [this] { OpenOverlay(false); });
            return;
        }

        // The dock selects the row it just added, and the selection is what puts a plan on the
        // canvas: one path to the screen, so the list and the drawing cannot disagree.
        for (int index = 0; index < plans.size(); ++index)
        {
            m_plans.append(plans.at(index));

            // The lining belongs to the SAME row: it is the same size, cut in the other cloth, and
            // the row is the size. A plan that is not `ok` is a size nobody lined — nothing to keep.
            if (index < lining.size() && lining.at(index).ok)
            {
                m_liningPlans.insert(m_plans.size() - 1, lining.at(index));
            }

            m_runs->AddRun(plans.at(index));
        }
    });
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::GeneratePrint(const QStringList &sizeIds, const QString &modelToken,
                                  int columns, double marginMm, double gapMm, bool plainOutline)
{
    const ImportedDocument document = m_overlay != nullptr ? m_overlay->document() : m_document;
    if (!document.isValid())
    {
        return;
    }

    m_document = document;
    m_sizeIds = sizeIds;
    m_pieceIds = m_overlay != nullptr ? m_overlay->pieceIds() : m_pieceIds;

    // ── Les groupes que le run garde ENTIERS ─────────────────────────────────────────────────────
    //
    // Décision de Sophie (25-10-2026) : une seule dialog pour tout le run, et dedans une boîte par
    // groupe complet. Dans le PDF d'origine les membres d'un groupe sont superposés ; elle demande,
    // groupe par groupe, s'ils doivent le rester. « Yes » — coché d'avance — ne change rien ; « No »
    // dégroupe : chaque membre devient une pièce ordinaire, ses enfants et son empreinte à lui.
    //
    // ⚠️ **Avant le run, jamais pendant.** La question porte sur ce qui va être posé ; la poser une
    // fois le calcul lancé demanderait de le refaire, et un run d'impression se compte en minutes.
    const QVector<CompleteGroup> wholeGroups =
        CompleteGroups(document, m_pieceIds);
    QSet<QString> ungrouped;
    if (!wholeGroups.isEmpty())
    {
        GroupOverlapDialog dialog(wholeGroups, this);
        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }
        ungrouped = dialog.Ungrouped();
    }

    const ExportPrintModel *model = FindExportPrintModel(modelToken);
    if (model == nullptr)
    {
        model = FindExportPrintModel(QStringLiteral("A0Roll"));
    }

    const PrintRecipe recipe = ReadPrintRecipe(document.payload());
    const ExportTemplate decor = recipe.hasDecor ? recipe.decor : ExportTemplate::RabnestDefault();

    constexpr double kPxPerCm = 96.0 / 2.54;
    const double documentWidthMm = model->widthMm * static_cast<double>(std::max(columns, 1));
    const double binWidthPx = documentWidthMm / 10.0 * kPxPerCm;

    const CoverSeat seat = ExportCoverPage::SeatOf(model);
    const bool hasCover = seat != CoverSeat::None && !recipe.coverSvg.isEmpty();
    const double marginPx = (marginMm / 10.0) * kPxPerCm;
    const QRectF coverBlock =
        hasCover && seat == CoverSeat::InSheet ? ExportCoverPage::BlockPx(seat, marginPx) : QRectF();

    // The same moment of waiting the cutting plan shows, and the same second count: a print run
    // takes seconds to minutes, and a window that says nothing for that long is a window somebody
    // clicks on twice.
    auto *busy = new BusyDialog(this, tr("Print"), tr("Laying the print out…"));
    busy->show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    auto *clock = new QElapsedTimer();
    clock->start();

    // ⚠️ **On a worker thread, and not on the one that paints.** The print nesting is the same
    // engine as the cutting plan's — seconds of work, and anything that blocks the event loop for
    // that long paints NOTHING: not the dialogue that says it is working, not the seconds that
    // count it. `ImportOverlay::Generate` does the same for the other nesting, for the same reason.
    const QStringList pieces = m_pieceIds;

    auto *watcher = new QFutureWatcher<PrintPlan>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, clock, busy, decor, recipe, model, seat, hasCover, marginPx, coverBlock,
             document, sizeIds, documentWidthMm] {
                PrintPlan plan = watcher->future().result();

                // The figure the user reads is the time HER plan cost: measured around the engine,
                // and it travels inside the plan so that the canvas can write it.
                plan.seconds = clock->elapsed() / 1000.0;

                delete clock;
                watcher->deleteLater();
                busy->deleteLater();

                m_overlay->deleteLater();
                m_overlay = nullptr;

                if (!plan.ok)
                {
                    QMessageBox::warning(this, tr("Print"), plan.error);
                    return;
                }

                PrintOutput output;
                output.plan = plan;
                output.decor = decor;
                output.coverSvg = recipe.coverSvg;
                output.framed = model->hasFrame;
                output.sourcePath = document.path();

                if (hasCover)
                {
                    if (plan.grid.valid)
                    {
                        output.coverPaper =
                            ExportCoverPage::PaperPx(seat, marginPx, plan.grid.planRect,
                                                     plan.grid.cellWidthPx, plan.grid.cellHeightPx);
                        output.coverPage =
                            ExportCoverPage::PagePx(seat, marginPx, plan.grid.planRect,
                                                    plan.grid.cellWidthPx, plan.grid.cellHeightPx);
                    }
                    else if (seat == CoverSeat::InSheet)
                    {
                        output.coverPage = coverBlock;
                    }
                }

                // The row that presents it: the sizes it covers, then the paper it is printed on.
                //
                // **A range when the sizes are a range, every name when they are not.** Three sizes
                // or more that stand next to each other in the drawing's own layer list read better
                // as « Taille 32 to Taille 36 » than as the three of them strung out; as soon as one
                // is missing — a size kept out, a size the range does not have — the list is what
                // says the truth, and a range would show two boundaries that mean something else.
                const QString format = QCoreApplication::translate("OExportPrintModel", model->label);
                const QString sizes = SizesPhrase(document, sizeIds);

                const QString name =
                    sizes.isEmpty() ? format : tr("%1 - %2").arg(sizes, format);

                CutPlan row;
                row.sizeLabel = name;
                row.fabricWidthCm = documentWidthMm / 10.0;
                row.binWidthPx = plan.binWidthPx;
                row.usedHeightPx = plan.usedHeightPx;
                row.ok = true;

                const int index = m_plans.size();
                m_plans.append(row);
                m_printOutputs.insert(index, output);
                m_runs->SetCanGenerate(true);
                m_runs->AddRun(row, RunsDock::Kind::Print);
            });

    watcher->setFuture(QtConcurrent::run([document, sizeIds, pieces, marginCm = marginMm / 10.0,
                                          gapCm = gapMm / 10.0, binWidthPx,
                                          columns = std::max(columns, 1),
                                          sheetHeightMm = model->heightMm, coverBlock,
                                          token = QString::fromLatin1(model->token), plainOutline,
                                          ungrouped] {
        return PrintPlan::Generate(document, sizeIds, pieces, marginCm, gapCm, binWidthPx, columns,
                                   sheetHeightMm, coverBlock, token, plainOutline, ungrouped);
    }));
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::GenerateProjector(const QStringList &sizeIds, double marginCm, double gapCm)
{
    const ImportedDocument document = m_overlay != nullptr ? m_overlay->document() : m_document;
    if (!document.isValid())
    {
        return;
    }

    m_document = document;
    m_sizeIds = sizeIds;
    m_pieceIds = m_overlay != nullptr ? m_overlay->pieceIds() : m_pieceIds;

    // ⚠️ **Aucune question de groupe ici**, et ce n'est pas un oubli : pour le projecteur, les
    // groupes n'existent pas — chaque membre est une pièce classique, posée à côté de l'autre avec
    // son propre dessin. La recherche les décompose tous, sans rien demander.

    // The same moment of waiting the other runs show: a search tries up to eight widths, and a
    // window that says nothing for that long is a window somebody clicks on twice.
    auto *busy = new BusyDialog(this, tr("Projector"), tr("Looking for the smallest page…"));
    busy->show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    auto *clock = new QElapsedTimer();
    clock->start();

    const QStringList pieces = m_pieceIds;

    Projector::Limits limits;
    limits.gapCm = gapCm;

    auto *watcher = new QFutureWatcher<Projector::Outcome>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, clock, busy, document, sizeIds] {
                Projector::Outcome outcome = watcher->future().result();

                // The figure the user reads is the time HER plan cost, measured around the search —
                // and it travels inside the plan so that the canvas and the sheet can both write it.
                outcome.plan.seconds = clock->elapsed() / 1000.0;

                delete clock;
                watcher->deleteLater();
                busy->deleteLater();

                m_overlay->deleteLater();
                m_overlay = nullptr;

                if (!outcome.ok)
                {
                    QMessageBox::warning(this, tr("Projector"), outcome.error);
                    return;
                }

                ProjectorOutput output;
                output.plan = outcome.plan;
                output.sourcePath = document.path();

                // The row that presents it: the sizes it covers. **Pas de format, pas de papier** —
                // une projection n'a ni l'un ni l'autre, et le nom de sa ligne ne doit pas en
                // inventer.
                const QString sizes = SizesPhrase(document, sizeIds);

                CutPlan row;
                row.sizeLabel = sizes.isEmpty() ? tr("All sizes") : sizes;
                row.binWidthPx = outcome.plan.binWidthPx;
                row.usedHeightPx = outcome.plan.usedHeightPx;
                row.marginCm = outcome.plan.marginCm;
                row.gapCm = outcome.plan.gapCm;
                row.seconds = outcome.plan.seconds;
                row.ok = true;

                const int index = m_plans.size();
                m_plans.append(row);
                m_projectorOutputs.insert(index, output);
                m_runs->SetCanGenerate(true);
                m_runs->AddRun(row, RunsDock::Kind::Projector);
            });

    watcher->setFuture(QtConcurrent::run(
        [document, sizeIds, pieces, marginCm, limits] {
            return Projector::Nest(document, sizeIds, pieces, marginCm, limits);
        }));
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ExportProjectorOutput(int index)
{
    const ProjectorOutput &output = m_projectorOutputs.value(index);
    const CutPlan &row = m_plans.at(index);

    // The folder proposed is the one the PDF came from, and the name proposed is the row's own: the
    // file she is about to write is the one she pressed.
    ExportPdfDialog dialog(RunsDock::Name(row, RunsDock::Kind::Projector),
                           QFileInfo(output.sourcePath).absolutePath(), this,
                           tr("Export the projection as PDF"));
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString path = QDir(dialog.directory()).filePath(dialog.fileName());

    // The payload is in the PDF the run was read from, and that PDF is the only thing that still
    // knows what the run was drawn from.
    const ImportedDocument document = ImportedDocument::Read(output.sourcePath);
    if (!document.isValid() || document.kind() != ImportedDocument::Kind::Ostara)
    {
        QMessageBox::warning(
            this, tr("Export"),
            tr("The PDF this projection was laid from can no longer be read:\n%1\n\n%2")
                .arg(output.sourcePath, document.error()));
        return;
    }

    PrintPdf::Job job;
    job.plan = output.plan;
    job.payload = document.payload();
    job.projector = true;
    job.overlays = Projector::Overlays();
    job.sourcePath = output.sourcePath;
    job.title = RunsDock::Name(row, RunsDock::Kind::Projector);

    QString error;
    if (!PrintPdf::Write(job, path, &error))
    {
        QMessageBox::warning(this, tr("Export"), error);
        return;
    }

    QMessageBox::information(this, tr("Export"), tr("File written:\n%1").arg(path));
}
