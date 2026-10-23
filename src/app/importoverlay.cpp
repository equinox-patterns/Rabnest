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

#include "app/checklistpage.h"
#include "app/cutplanpage.h"
#include "app/importpage.h"
#include "app/layerspage.h"
#include "core/layerreducer.h"
#include "core/pattern.h"

#include <QApplication>
#include <QDialog>
#include <QTimer>
#include <QElapsedTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QStackedWidget>
#include <QVBoxLayout>

/// What the window says while the pieces are being laid out — **a dialogue, not a bar in a corner.**
///
/// A run of two sizes took half a minute on a real pattern, and thirty seconds of a window that says
/// nothing is thirty seconds of somebody clicking twice. It is a small modal dialogue: it takes the
/// focus, it cannot be missed, and it says which size it is on and how long it has been running.
class BusyDialog : public QDialog
{
public:
    BusyDialog(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Rabnest"));
        setModal(true);
        setWindowFlag(Qt::WindowContextHelpButtonHint, false);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 20, 24, 20);
        layout->setSpacing(12);

        m_label = new QLabel(tr("Laying the pieces out on the fabric…"), this);
        m_label->setWordWrap(true);
        layout->addWidget(m_label);

        m_bar = new QProgressBar(this);
        m_bar->setRange(0, 0);
        m_bar->setTextVisible(false);
        layout->addWidget(m_bar);

        m_elapsed = new QLabel(tr("0.0 s"), this);
        layout->addWidget(m_elapsed);

        setFixedWidth(400);

        // ⚠️ **A clock that only moves when a size finishes is a clock that looks stopped.** A size
        // takes seconds, so the figure would sit still for exactly as long as somebody doubts it.
        // The second count ticks on its own, ten times a second, from the moment the dialogue opens.
        m_clock.start();
        m_tick.setInterval(100);
        connect(&m_tick, &QTimer::timeout, this, [this] {
            m_elapsed->setText(tr("%1 s").arg(m_clock.elapsed() / 1000.0, 0, 'f', 1));
        });
        m_tick.start();
    }

    /// How far the run has got: one step per size, and the seconds it has taken so far.
    void SetProgress(int done, int total)
    {
        if (total > 0)
        {
            m_bar->setRange(0, total);
            m_bar->setValue(done);
            m_label->setText(tr("Laying the pieces out on the fabric — size %1 of %2…")
                                 .arg(done)
                                 .arg(total));
        }
    }

private:
    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_elapsed = nullptr;
    QElapsedTimer m_clock;
    QTimer m_tick;
};
namespace
{
/// The card's own size: smaller than the window on every side, so the workspace stays visible around
/// it and the panel reads as something laid ON the interface rather than another window beside it.
constexpr int kCardWidth = 760;
constexpr int kCardHeight = 660;
constexpr int kCardInset = 48;


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
        Finish({});
        return;
    }

    QWidget::keyPressEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::Finish(const QVector<CutPlan> &plans)
{
    emit Finished(plans);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::StartAtCutPlan(const ImportedDocument &document, const QStringList &sizeIds,
                                   const QStringList &pieceIds)
{
    m_document = document;
    m_sizeIds = sizeIds;
    m_pieceIds = pieceIds;

    ShowCutPlan();
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::ShowPage(QWidget *page)
{
    m_stack->setCurrentWidget(page);
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
    m_cutPlan->SetSizes(sizes);
    ShowPage(m_cutPlan);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportOverlay::Generate()
{
    m_cutPlan->ClearError();

    const CutPlanRequest request = m_cutPlan->Request();
    const QStringList sizes = m_cutPlan->CheckedSizeIds();

    // The payload is small and is read once, on this thread: what is long is the laying out, and
    // that is what gets its own thread below.
    QVector<CutSize> loaded;
    QString error;
    if (!Pattern::Load(m_document, sizes, m_pieceIds, request.foldedFabric, loaded, &error))
    {
        m_cutPlan->ShowError(error);
        return;
    }

    m_busy = new BusyDialog(this);
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

    auto *watcher = new QFutureWatcher<CutPlan>(this);

    connect(watcher, &QFutureWatcherBase::progressValueChanged, this,
            [this, watcher, clock](int done) {
                if (m_busy != nullptr)
                {
                    m_busy->SetProgress(done, watcher->progressMaximum());
                }
            });

    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, clock, pool] {
        const QVector<CutPlan> plans = watcher->future().results();

        const double seconds = clock->elapsed() / 1000.0;
        delete clock;
        pool->deleteLater();
        watcher->deleteLater();

        if (m_busy != nullptr)
        {
            m_busy->deleteLater();
            m_busy = nullptr;
        }

        // One failure fails the run: a panel that closed on a half-made set of plans would leave her
        // with runs she did not ask for and no way to say which.
        for (const CutPlan &plan : plans)
        {
            if (!plan.ok)
            {
                m_cutPlan->ShowError(plan.error);
                return;
            }
        }

        Q_UNUSED(seconds)
        Finish(plans);
    });

    // One job per size, and the futures are read in the order the sizes were asked for.
    watcher->setFuture(QtConcurrent::mapped(pool, loaded, [request](const CutSize &size) {
        // The figure the user reads is the time HER plan cost, so each one is measured on its own,
        // around the engine, and it travels inside the plan.
        QElapsedTimer one;
        one.start();

        CutPlan plan = CutPlan::Generate(size, request, size.decor);
        plan.seconds = one.elapsed() / 1000.0;
        return plan;
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
    Finish({});
}
