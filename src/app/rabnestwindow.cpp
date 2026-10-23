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

#include "app/importoverlay.h"
#include "app/outputinfodialog.h"
#include "app/runsdock.h"
#include "app/workspacepage.h"

#include <QAbstractButton>
#include <QAction>
#include <QCoreApplication>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>
#include "app/print/printpanel.h"
#include "core/print/exportcoverpage.h"
#include "core/print/printmetrics.h"
#include "core/print/printrecipe.h"

#include <QDockWidget>
#include <QApplication>
#include <QProgressDialog>
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
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
RabnestWindow::RabnestWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(Title());
    resize(1240, 820);

    m_workspace = new WorkspacePage(this);
    setCentralWidget(m_workspace);

    m_runs = new RunsDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_runs);

    connect(m_runs, &RunsDock::HomeRequested, this, &RabnestWindow::BackHome);
    connect(m_runs, &RunsDock::GenerateRequested, this, &RabnestWindow::GenerateOutput);
    connect(m_runs, &RunsDock::InfoRequested, this, &RabnestWindow::ShowOutputInfo);
    connect(m_runs, &RunsDock::DeleteRequested, this, &RabnestWindow::DeleteOutput);
    // Export is not wired yet: the signal exists, the dock says so in its tooltip, and nothing is
    // connected — a gesture that did half of something would be worse than one that is visibly not
    // there yet. Print is wired, and to the print panel beside the canvas.

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

        if (index < m_plans.size())
        {
            m_workspace->Show(m_plans.at(index));
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

    // The map is keyed by ROW, so every key above the one removed moves down with its row.
    QHash<int, PrintOutput> reindexed;
    for (auto it = m_printOutputs.cbegin(); it != m_printOutputs.cend(); ++it)
    {
        reindexed.insert(it.key() > index ? it.key() - 1 : it.key(), it.value());
    }
    m_printOutputs = reindexed;

    m_runs->RemoveRun(index);

    if (m_plans.isEmpty())
    {
        m_workspace->Clear();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::ShowOutputInfo(int index)
{
    if (index < 0 || index >= m_plans.size())
    {
        return;
    }

    OutputInfoDialog dialog(m_plans.at(index), this);
    dialog.exec();
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::BackHome()
{
    // What is lost, named: the PDF she imported, not « the current file ».
    const QString file = QFileInfo(m_document.path()).fileName();

    if (m_document.isValid())
    {
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

    // Nothing of the previous PDF survives: not the plans, not the rows, not the drawing, not the
    // document the next output would have been generated from.
    m_plans.clear();
    m_runs->Clear();
    m_workspace->Clear();

    m_document = ImportedDocument();
    m_sizeIds.clear();
    m_pieceIds.clear();
    m_runs->SetCanGenerate(false);

    // The print panel belonged to a document that no longer exists.
    m_printIndex = -1;

    OpenOverlay(false);
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

    connect(m_overlay, &ImportOverlay::Finished, this, [this](const QVector<CutPlan> &plans) {
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

        // The dock selects the row it just added, and the selection is what puts a plan on the
        // canvas: one path to the screen, so the list and the drawing cannot disagree.
        for (const CutPlan &plan : plans)
        {
            m_plans.append(plan);
            m_runs->AddRun(plan);
        }
    });
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestWindow::GeneratePrint(const QStringList &sizeIds, const QString &modelToken,
                                  int columns, double marginMm, double gapMm)
{
    const ImportedDocument document = m_overlay != nullptr ? m_overlay->document() : m_document;
    if (!document.isValid())
    {
        return;
    }

    m_document = document;
    m_sizeIds = sizeIds;
    m_pieceIds = m_overlay != nullptr ? m_overlay->pieceIds() : m_pieceIds;

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

    // The same moment of waiting the cutting plan shows: a print run takes seconds to minutes, and
    // a spinning cursor says neither what is happening nor that anything is.
    QProgressDialog waiting(tr("Laying the print out…"), QString(), 0, 0, this);
    waiting.setWindowTitle(tr("Print"));
    waiting.setWindowModality(Qt::WindowModal);
    waiting.setCancelButton(nullptr);
    waiting.setMinimumDuration(0);
    waiting.show();
    QApplication::processEvents();

    const PrintPlan plan = PrintPlan::Generate(document, sizeIds, m_pieceIds, marginMm / 10.0,
                                               gapMm / 10.0, binWidthPx, std::max(columns, 1),
                                               model->heightMm, coverBlock);

    waiting.close();

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

    if (hasCover)
    {
        if (plan.grid.valid)
        {
            output.coverPaper = ExportCoverPage::PaperPx(seat, marginPx, plan.grid.planRect,
                                                         plan.grid.cellWidthPx, plan.grid.cellHeightPx);
            output.coverPage = ExportCoverPage::PagePx(seat, marginPx, plan.grid.planRect,
                                                       plan.grid.cellWidthPx, plan.grid.cellHeightPx);
        }
        else if (seat == CoverSeat::InSheet)
        {
            output.coverPage = coverBlock;
        }
    }

    // The row that presents it: a print run is named by the sizes it covers, as Ostara's own
    // dialog names the generation it makes.
    QStringList labels;
    for (const ImportedLayer &layer : document.layers())
    {
        if (sizeIds.contains(layer.id))
        {
            labels.append(layer.label);
        }
    }

    const QString name = labels.size() > 1
                             ? tr("Sizes %1 - Print").arg(labels.join(QStringLiteral("–")))
                             : tr("%1 - Print").arg(labels.value(0));

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
    m_runs->AddRun(row);
}
