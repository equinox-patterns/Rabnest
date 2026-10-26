/************************************************************************
 **
 **  @file   importoverlay.cpp
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

#include "app/importoverlay.h"

#include "app/busydialog.h"
#include "app/checklistpage.h"
#include "app/cutplanpage.h"
#include "app/print/printpanel.h"
#include "app/importpage.h"
#include "app/layerspage.h"
#include "core/layerreducer.h"
#include "core/pattern.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace
{
/// The card's own size: smaller than the window on every side, so the workspace stays visible around
/// it and the panel reads as something laid ON the interface rather than another window beside it.
constexpr int kCardWidth = 760;
constexpr int kCardHeight = 660;
constexpr int kCardInset = 48;

/// What one size of a run gives: its plan, and — when the run asked for a pair — the lining's.
///
/// **The two travel together because they are one size's answer.** A lined garment is cut twice, and
/// the row of the dock is the size: two lists could drift apart by a bad index, one struct cannot.
struct RunPlans
{
    CutPlan fabric;
    CutPlan lining;
    bool hasLining = false;
};


//---------------------------------------------------------------------------------------------------------------------
void Pump()
{
    // The laying out runs on this thread — the engine has no abort hook of its own and a plan is
    // seconds — so the panel is painted by hand, once, before the work starts and between sizes.
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
ImportOverlay::ImportOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, false);

    // The card is a child, so it takes the clicks; the overlay swallows the rest, and the workspace
    // underneath stops answering while the journey is up.
    //
    // ⚠️ **A QFrame with no rule of its own paints no background at all** — only its frame line,
    // which is exactly what made the panel invisible and the whole window a flat grey. The surface is
    // stated here rather than in the theme because the theme is GENERATED from Ostara's palette and
    // a rule typed into it would be overwritten; the colours are taken from the palette so the two
    // cannot drift, and the border is written with the background as a styled widget requires.
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("importCard"));
    m_card->setFrameShape(QFrame::NoFrame);
    m_card->setAutoFillBackground(true);
    m_card->setBackgroundRole(QPalette::Window);
    m_card->setStyleSheet(QStringLiteral("QFrame#importCard {"
                                         "  background-color: palette(window);"
                                         "  border: 1px solid palette(mid);"
                                         "  border-radius: 10px;"
                                         "}"));

    // A shadow, because a card lying on a scrim without one reads as a hole rather than as a panel
    // above the interface.
    auto *shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 90));
    m_card->setGraphicsEffect(shadow);

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(m_card);
    cardLayout->addWidget(m_stack);

    // ── Annuler ────────────────────────────────────────────────────────────────────────────────
    //
    // **Sortir à tout moment, d'un clic, et sans rien garder.** Échap le fait déjà au clavier,
    // mais une fenêtre dont la seule sortie est une touche qu'on ne voit pas n'en a pas : la
    // génération d'une sortie peut se quitter au milieu des trois étapes, et ce bouton le dit.
    // Il n'existe pas à l'étape d'import : là, il n'y a rien à annuler.
    m_cancel = new QPushButton(tr("Cancel"), m_card);
    m_cancel->setObjectName(QStringLiteral("cancelButton"));
    m_cancel->setCursor(Qt::PointingHandCursor);

    auto *cancelRow = new QHBoxLayout();
    cancelRow->setContentsMargins(16, 0, 16, 14);
    cancelRow->addStretch(1);
    cancelRow->addWidget(m_cancel);
    cardLayout->addLayout(cancelRow);

    connect(m_cancel, &QPushButton::clicked, this, [this] { Finish({}, {}); });

    m_import = new ImportPage(m_stack);
    m_sizes = new ChecklistPage(tr("Sizes"), tr("Tick the sizes to keep."), m_stack);
    m_pieces = new ChecklistPage(tr("Pieces"), tr("Tick the pieces to keep."), m_stack);
    m_cutPlan = new CutPlanPage(m_stack);
    m_layers = new LayersPage(m_stack);

    m_sizes->SetBackVisible(true);

    m_stack->addWidget(m_import);
    m_stack->addWidget(m_sizes);
    m_stack->addWidget(m_pieces);
    m_stack->addWidget(m_cutPlan);
    m_stack->addWidget(m_layers);

    connect(m_import, &ImportPage::FileChosen, this, &ImportOverlay::ChooseFile);
    connect(m_sizes, &ChecklistPage::Continue, this, &ImportOverlay::ShowPieces);
    connect(m_sizes, &ChecklistPage::Back, this, [this] { ShowPage(m_import); });
    connect(m_pieces, &ChecklistPage::Continue, this, &ImportOverlay::ShowCutPlan);
    connect(m_pieces, &ChecklistPage::Back, this, &ImportOverlay::ShowSizes);
    connect(m_cutPlan, &CutPlanPage::Continue, this, &ImportOverlay::Generate);
    connect(m_cutPlan, &CutPlanPage::Back, this, [this] { ShowPage(m_pieces); });
    connect(m_layers, &LayersPage::Back, this, [this] { ShowPage(m_import); });
    connect(m_layers, &LayersPage::Export, this, &ImportOverlay::ReduceLayers);

    ShowPage(m_import);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    // The scrim: dark enough that the card reads as being over the interface, light enough that the
    // runs and the canvas are still recognisable underneath.
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 80));
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    const int width = std::min(kCardWidth, std::max(320, this->width() - 2 * kCardInset));
    const int height = std::min(kCardHeight, std::max(240, this->height() - 2 * kCardInset));

    m_card->setGeometry((this->width() - width) / 2, (this->height() - height) / 2, width, height);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        Finish({}, {});
        return;
    }

    QWidget::keyPressEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::Finish(const QVector<CutPlan> &plans, const QVector<CutPlan> &lining, bool reduced)
{
    emit Finished(plans, lining, reduced);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::StartAtSizes(const ImportedDocument &document)
{
    m_document = document;
    // Nothing is carried over but the document: the sizes and the pieces of the previous output
    // are the previous output's answer, and asking them again is what « generate an output »
    // means. The pages are refilled from the document as the journey goes.
    m_sizeIds.clear();
    m_pieceIds.clear();

    ShowSizes();
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ShowPage(QWidget *page)
{
    m_stack->setCurrentWidget(page);
    // Nothing to cancel before a PDF has been chosen: the import page IS the way in.
    m_cancel->setVisible(page != m_import);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
    }

    QWidget::changeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::Retranslate()
{
    m_cancel->setText(tr("Cancel"));

    // The two questions, and not the pages' own words: a checklist page knows how to say its buttons,
    // but only this overlay knows that this one asks about sizes and that one about pieces.
    m_sizes->SetTexts(tr("Sizes"), tr("Tick the sizes to keep."));
    m_pieces->SetTexts(tr("Pieces"), tr("Tick the pieces to keep."));
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ChooseFile(const QString &path)
{
    m_import->ClearError();
    m_document = ImportedDocument::Read(path);
    m_sizeIds.clear();

    if (!m_document.isValid())
    {
        m_import->ShowError(m_document.error());
        return;
    }

    // Ce que le PDF EST, dit une fois : c'est lui qui décide l'avertissement de format des deux
    // onglets (le cartouche est un dessin, ses mots ne se réécrivent pas).
    m_cutPlan->SetDocumentKind(m_document.isProjector());

    if (m_document.kind() == ImportedDocument::Kind::Foreign)
    {
        if (m_document.layers().isEmpty())
        {
            m_import->ShowError(tr("This PDF does not come from Ostara and carries no layer: there is "
                                   "nothing to reduce."));
            return;
        }

        m_layers->SetDocument(m_document);
        ShowPage(m_layers);
        return;
    }

    ShowSizes();
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ShowSizes()
{
    if (m_document.layers().isEmpty())
    {
        // A single-size run has nothing to switch between: the journey starts at the pieces.
        ShowPieces();
        return;
    }

    QVector<QPair<QString, QString>> items;
    items.reserve(m_document.layers().size());
    for (const ImportedLayer &layer : m_document.layers())
    {
        items.append({layer.id, layer.label});
    }

    m_sizes->SetItems(items);
    ShowPage(m_sizes);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ShowPieces()
{
    m_sizeIds = m_sizes->CheckedIds();

    const QStringList pieces = m_document.Pieces(m_sizeIds);

    QVector<QPair<QString, QString>> items;
    items.reserve(pieces.size());
    for (const QString &piece : pieces)
    {
        items.append({piece, piece});
    }

    m_pieces->SetItems(items);
    ShowPage(m_pieces);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ShowCutPlan()
{
    m_pieceIds = m_pieces->CheckedIds();

    // The sizes offered here are the ones the first step KEPT, with their words: a plan is laid for
    // one size, and asking which of the kept sizes get one is a different question from which sizes
    // the pattern has.
    QVector<QPair<QString, QString>> sizes;
    for (const ImportedLayer &layer : m_document.layers())
    {
        if (m_sizeIds.contains(layer.id))
        {
            sizes.append({layer.id, layer.label});
        }
    }

    m_cutPlan->ClearError();
    // A lining plan is offered when the pattern has a piece cut in the lining — and only then.
    m_cutPlan->SetLiningAvailable(m_document.HasLiningPieces(m_sizeIds));
    m_cutPlan->SetSizes(sizes);
    ShowPage(m_cutPlan);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::Generate()
{
    m_cutPlan->ClearError();

    // The button obeys the tab: a print run is not a cutting plan, and generating one from the
    // other's settings is exactly what she saw happen.
    if (m_cutPlan->ActiveTab() == CutPlanPage::Tab::Print)
    {
        PrintPanel *panel = m_cutPlan->PrintSettings();
        if (panel != nullptr)
        {
            emit PrintRequested(m_cutPlan->CheckedSizeIds(),
                                panel->Model() != nullptr
                                    ? QString::fromLatin1(panel->Model()->token)
                                    : QStringLiteral("A0Roll"),
                                panel->Columns(), panel->MarginMm(), panel->GapMm(),
                                panel->PlainOutline());
        }
        return;
    }

    if (m_cutPlan->ActiveTab() == CutPlanPage::Tab::Projector)
    {
        const CutPlanPage::ProjectorSettings settings = m_cutPlan->Projector();
        emit ProjectorRequested(m_cutPlan->CheckedSizeIds(), settings.marginCm, settings.gapCm);
        return;
    }

    const CutPlanRequest request = m_cutPlan->Request();
    const QStringList sizes = m_cutPlan->CheckedSizeIds();
    const bool lining = m_cutPlan->IsLining();

    // ── La doublure : la même requête, une autre laize ─────────────────────────────────────────
    //
    // **Rien d'autre ne change.** Le pli, le sens, la marge et l'écart sont ceux du run et valent
    // pour les deux nappes ; seule la largeur est propre à la doublure, parce qu'elle s'achète sur
    // son propre rouleau. C'est la même leçon que la requête d'Ostara : `documentLiningWidthMm` est
    // le seul champ qu'une paire ajoute.
    CutPlanRequest liningRequest = request;
    liningRequest.widthCm = m_cutPlan->LiningWidthCm();

    const Cloth cloth = request.foldedFabric ? Cloth::Folded : Cloth::Flat;

    // The payload is small and is read once, on this thread: what is long is the laying out, and
    // that is what gets its own thread below.
    QVector<CutSize> loaded;
    QString error;
    if (!Pattern::Load(m_document, sizes, m_pieceIds, cloth, loaded, &error, Material::Fabric))
    {
        m_cutPlan->ShowError(error);
        return;
    }

    // Le jeu de pièces de la doublure, lu dans SA matière : une pièce sans quantité de doublure n'y
    // entre pas, et une pièce taillée dans les deux étoffes y entre avec sa propre quantité. Une
    // taille sans pièce de doublure n'a pas de plan de doublure — elle garde son rang, vide.
    QVector<CutSize> liningLoaded;
    if (lining && !Pattern::Load(m_document, sizes, m_pieceIds, cloth, liningLoaded, &error,
                                 Material::Lining))
    {
        m_cutPlan->ShowError(error);
        return;
    }

    m_busy = new BusyDialog(this, QStringLiteral("Rabnest"),
                            lining ? tr("Laying the pieces out on the fabric and the lining…")
                                   : tr("Laying the pieces out on the fabric…"),
                            lining ? tr("Laying the pieces out — size %1 of %2…")
                                   : tr("Laying the pieces out on the fabric — size %1 of %2…"));
    m_busy->SetProgress(0, loaded.size());
    m_busy->show();
    Pump();

    auto *clock = new QElapsedTimer();
    clock->start();

    // ⚠️ **On a worker thread, and not on the one that paints.** A run of eight sizes takes seconds,
    // and anything that blocks the event loop for seconds paints NOTHING — not the panel that says
    // it is working, not the bar that counts it. That is why a busy dialogue over a synchronous run
    // looks like a frozen window.
    auto *pool = new QThreadPool(this);
    pool->setMaxThreadCount(1);  // one size at a time: the same plan as a sequential run, always

    auto *watcher = new QFutureWatcher<RunPlans>(this);

    connect(watcher, &QFutureWatcherBase::progressValueChanged, this,
            [this, watcher, clock](int done) {
                if (m_busy != nullptr)
                {
                    m_busy->SetProgress(done, watcher->progressMaximum());
                }
            });

    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, clock, pool] {
        const QVector<RunPlans> runs = watcher->future().results();

        const double seconds = clock->elapsed() / 1000.0;
        delete clock;
        pool->deleteLater();
        watcher->deleteLater();

        if (m_busy != nullptr)
        {
            m_busy->deleteLater();
            m_busy = nullptr;
        }

        QVector<CutPlan> plans;
        QVector<CutPlan> liningPlans;
        plans.reserve(runs.size());
        liningPlans.reserve(runs.size());

        // One failure fails the run: a panel that closed on a half-made set of plans would leave her
        // with runs she did not ask for and no way to say which. The lining fails its own size the
        // same way — a pair half laid out is not a pair.
        for (const RunPlans &run : runs)
        {
            if (!run.fabric.ok)
            {
                m_cutPlan->ShowError(run.fabric.error);
                return;
            }

            if (run.hasLining && !run.lining.ok)
            {
                m_cutPlan->ShowError(run.lining.error);
                return;
            }

            plans.append(run.fabric);

            // ⚠️ **The lists stay PARALLEL, empty plans included.** A size with nothing to line keeps
            // its rank here and is simply not stored by the window: two lists read by the same index
            // must be the same length, or a size would be shown its neighbour's lining.
            liningPlans.append(run.lining);
        }

        Q_UNUSED(seconds)
        Finish(plans, liningPlans);
    });

    // One job per size, and the futures are read in the order the sizes were asked for. A pair does
    // its two passes here, one after the other, so that a size is ONE unit of work: the bar counts
    // sizes, and a lining pass that had its own count would tell her she is on size 11 of 16.
    watcher->setFuture(QtConcurrent::mapped(
        pool, loaded, [request, liningRequest, liningLoaded, lining](const CutSize &size) {
            // The figure the user reads is the time HER plan cost, so each one is measured on its
            // own, around the engine, and it travels inside the plan.
            QElapsedTimer one;
            one.start();

            RunPlans out;
            out.fabric = CutPlan::Generate(size, request, size.decor);
            out.fabric.seconds = one.elapsed() / 1000.0;

            if (!lining)
            {
                return out;
            }

            // Its own pieces for the SAME size — found by the label, which is what names a size on
            // both sides, and which the two loads carry in the same order anyway.
            const CutSize *liningSize = nullptr;
            for (const CutSize &candidate : liningLoaded)
            {
                if (candidate.label == size.label)
                {
                    liningSize = &candidate;
                    break;
                }
            }

            // A size nobody lined has no lining plan: not an empty sheet on the canvas, nothing.
            if (liningSize == nullptr || liningSize->poses.isEmpty())
            {
                return out;
            }

            QElapsedTimer two;
            two.start();
            out.lining = CutPlan::Generate(*liningSize, liningRequest, liningSize->decor);
            out.lining.seconds = two.elapsed() / 1000.0;
            out.hasLining = true;
            return out;
        }));
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ReduceLayers(const QString &target)
{
    QVector<int> keep;
    keep.reserve(m_layers->CheckedIds().size());
    for (const QString &id : m_layers->CheckedIds())
    {
        keep.append(id.toInt());
    }

    QString error;
    if (!LayerReducer::Reduce(m_document.path(), target, keep, &error))
    {
        m_layers->ShowError(error);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Rabnest"), tr("File written: %1").arg(target));

    // The file is written and there is nothing to show: the window goes back to « import a PDF ».
    Finish({}, {}, true);
}
