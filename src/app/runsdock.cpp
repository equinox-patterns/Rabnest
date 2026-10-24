/************************************************************************
 **
 **  @file   runsdock.cpp
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

#include "app/runsdock.h"

#include "app/rabnesttheme.h"

#include <QEvent>
#include <algorithm>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
constexpr int kIconPixels = 20;
constexpr int kActionColumnWidth = 40;
constexpr int kRowHeight = 36;

enum Column
{
    kName = 0,
    kInfo = 1,
    kExport = 2,
    kPrint = 3,
    kDelete = 4
};

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
/// One icon that knows both of its faces: the theme's, and the DARK one for a lit row.
///
/// A hovered or selected row is dark in both themes — the accent colour is dark — so an icon drawn
/// in the light set disappears on it. Ostara solved this with a delegate that swaps the mode; the
/// icon itself has to carry both files for that to be possible.
auto LitIcon(const QString &baseName) -> QIcon
{
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/light/%1_light.svg").arg(baseName), QSize(),
                 QIcon::Normal);
    icon.addFile(QStringLiteral(":/icons/dark/%1_dark.svg").arg(baseName), QSize(),
                 QIcon::Selected);
    return icon;
}

//---------------------------------------------------------------------------------------------------------------------
/// Paints a cell's icon centred, and picks its face from the row's state.
///
/// ⚠️ **The base paint is asked to draw NO icon.** A QStyledItemDelegate draws the decoration at the
/// left edge of the cell, so the centred icon would come out twice — one on each side. Clearing it
/// from the option keeps the selection background and the text, and leaves the icon to us.
class LitIconDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->icon = QIcon();
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (icon.isNull())
        {
            return;
        }

        const bool lit = (option.state & QStyle::State_Selected) != 0
                         || (option.state & QStyle::State_MouseOver) != 0;
        const QPixmap pixmap =
            icon.pixmap(kIconPixels, kIconPixels, lit ? QIcon::Selected : QIcon::Normal);

        const QRect target(option.rect.center().x() - kIconPixels / 2,
                           option.rect.center().y() - kIconPixels / 2, kIconPixels, kIconPixels);
        painter->drawPixmap(target, pixmap);
    }
};

//---------------------------------------------------------------------------------------------------------------------
auto MakeIconButton(const QString &baseName, const QString &text, QWidget *parent) -> QPushButton *
{
    auto *button = new QPushButton(RabnestTheme::Icon(baseName), text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setIconSize(QSize(kIconPixels, kIconPixels));
    return button;
}
}  // namespace

/// What an output is called: its size, and the kind of plan it is.
///
/// **The size alone is not a name.** Two outputs of the same size on two cloths are two different
/// answers, and both are called « Taille 46 - Cut » — that is deliberate: the dock is a list of
/// what she ASKED FOR, and what differs between them is a setting, which is exactly what the
/// information dialogue exists to show.
auto RunsDock::Name(const CutPlan &plan, bool isPrint) -> QString
{
    return isPrint ? plan.sizeLabel : QObject::tr("%1 - Cut").arg(plan.sizeLabel);
}

//---------------------------------------------------------------------------------------------------------------------
RunsDock::RunsDock(QWidget *parent)
    : QDockWidget(tr("Outputs"), parent)
{
    setObjectName(QStringLiteral("runsDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // **Not closable.** There is no menu bar to open it again, and a window whose only way back to
    // its outputs has been closed is a dead end. Movable and floatable stay: that costs nothing.
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    BuildUi();
    UpdateGlyphs();
    RefreshEnabledState();
    SetCanGenerate(false);
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::BuildUi()
{
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 6, 0, 8);
    layout->setSpacing(6);

    // ── Les deux gestes du haut ────────────────────────────────────────────────────────────────
    //
    // The same two as Ostara's outputs dock, in the same order, and for the same reason: making one
    // more output, and sending several away at once. The second is not wired yet and is disabled,
    // rather than quietly doing nothing when pressed.
    m_generate = MakeIconButton(QStringLiteral("stack-plus-fill"), tr("Generate an output"), body);
    m_generate->setObjectName(QStringLiteral("generateOutputButton"));
    m_exportSeveral =
        MakeIconButton(QStringLiteral("download-fill"), tr("Export several outputs"), body);
    m_exportSeveral->setObjectName(QStringLiteral("exportSeveralButton"));
    m_exportSeveral->setEnabled(false);
    m_exportSeveral->setToolTip(tr("Not available yet"));

    auto *top = new QHBoxLayout();
    top->setContentsMargins(8, 0, 8, 0);
    top->setSpacing(6);
    top->addWidget(m_generate);
    top->addWidget(m_exportSeveral);
    layout->addLayout(top);

    // ── Le tableau ─────────────────────────────────────────────────────────────────────────────
    m_table = new QTableWidget(0, 5, body);
    m_table->setObjectName(QStringLiteral("outputsTable"));
    m_table->setShowGrid(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    // The hover rule of the icons needs the view to KNOW it is hovered, and Qt only says so when
    // the mouse is tracked.
    m_table->setMouseTracking(true);
    m_table->viewport()->setMouseTracking(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(kName, QHeaderView::Stretch);
    for (int column : {kInfo, kExport, kPrint, kDelete})
    {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
        m_table->setColumnWidth(column, kActionColumnWidth);
    }
    m_table->setWordWrap(false);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto *icons = new LitIconDelegate(m_table);
    for (int column : {kInfo, kExport, kPrint, kDelete})
    {
        m_table->setItemDelegateForColumn(column, icons);
    }

    layout->addWidget(m_table, 1);

    // ── Le retour à l'accueil ──────────────────────────────────────────────────────────────────
    //
    // The old « Import a PDF » said nothing about what it costs. This one names the ship it sinks:
    // going home is the only gesture that throws the whole PDF away, outputs included.
    m_home = new QPushButton(tr("Back to home"), body);
    m_home->setObjectName(QStringLiteral("homeButton"));
    m_home->setCursor(Qt::PointingHandCursor);
    m_home->setToolTip(tr("Close this PDF and go back to the start; every output of it is lost"));

    auto *bottom = new QHBoxLayout();
    bottom->setContentsMargins(8, 0, 8, 0);
    bottom->addWidget(m_home);
    layout->addLayout(bottom);

    setWidget(body);

    m_glyphs = {{m_generate, QStringLiteral("stack-plus-fill")},
                {m_exportSeveral, QStringLiteral("download-fill")}};
    for (const Glyph &glyph : m_glyphs)
    {
        glyph.button->installEventFilter(this);
    }

    connect(m_generate, &QPushButton::clicked, this, &RunsDock::GenerateRequested);
    connect(m_exportSeveral, &QPushButton::clicked, this, &RunsDock::ExportSeveralRequested);
    connect(m_home, &QPushButton::clicked, this, &RunsDock::HomeRequested);

    // **The ROW decides, and not the cell.** Every row carries four icons, and clicking one of them
    // moves the table's current cell within a row that is already the current one. Announcing that as
    // a change re-shows the run from the dock's own copy — with the zoom thrown away and, before the
    // dock learned to take hand moves back, with every piece back where the nesting had put it: the
    // gesture looked undone by the very click that was meant to export it.
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row == m_currentRow)
        {
            return;
        }

        m_currentRow = row;
        emit CurrentChanged(row);
    });

    // One routing point: the column the click landed in decides the gesture, and the row decides
    // whose. A cell with no icon — the name — only selects, which is already handled above.
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        switch (column)
        {
        case kInfo:
            emit InfoRequested(row);
            break;
        case kExport:
            emit ExportRequested(row);
            break;
        case kPrint:
            emit PrintRequested(row);
            break;
        case kDelete:
            emit DeleteRequested(row);
            break;
        case kName:
        default:
            break;
        }
    });
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::AddRun(const CutPlan &plan, bool isPrint)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setRowHeight(row, kRowHeight);

    auto *name = new QTableWidgetItem(RunsDock::Name(plan, isPrint));
    name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    name->setToolTip(plan.ok ? RunsDock::Name(plan, isPrint) : plan.error);
    m_table->setItem(row, kName, name);

    struct Action
    {
        int column;
        QString icon;
        QString tooltip;
    };

    const QVector<Action> actions = {
        {kInfo, QStringLiteral("info"), tr("What this output was asked for")},
        // The two that are not wired yet say so, in the tooltip, rather than doing nothing in
        // silence when pressed.
        // A print run is not an image: it leaves as a PDF with one layer per size and one page per
        // sheet, and the tooltip is where that is said before anything is pressed.
        {kExport, QStringLiteral("download"),
         isPrint ? tr("Export this print as PDF") : tr("Export this plan as an image")},
        // A cutting plan prints as one sheet the plan is shrunk onto; a print run prints as its own
        // sheets, at their size. Two gestures, two sentences.
        {kPrint, QStringLiteral("printer"),
         isPrint ? tr("Print these sheets") : tr("Print this plan")},
        {kDelete, QStringLiteral("trash"), tr("Delete")}};

    for (const Action &action : actions)
    {
        auto *cell = new QTableWidgetItem();
        cell->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        cell->setIcon(LitIcon(action.icon));
        cell->setToolTip(action.tooltip);
        m_table->setItem(row, action.column, cell);
    }

    m_table->setCurrentCell(row, kName);
    RefreshEnabledState();
}

//---------------------------------------------------------------------------------------------------------------------
auto RunsDock::CurrentRow() const -> int
{
    return m_table->currentRow();
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::RemoveRun(int index)
{
    if (index < 0 || index >= m_table->rowCount())
    {
        return;
    }

    // The row is going, so whatever the table announces next is a change, even if it keeps the same
    // number: forgetting the announced row here is what lets that announcement through.
    m_currentRow = -1;

    m_table->removeRow(index);

    // **Le canevas ne doit pas garder la sortie qu'on vient d'effacer.** Qt ne choisit pas toujours
    // une ligne de remplacement après un retrait ; quand il n'en choisit aucune, on en choisit une,
    // et le changement de sélection fait le reste (c'est le seul chemin vers le canevas).
    if (m_table->rowCount() > 0 && m_table->currentRow() < 0)
    {
        m_table->setCurrentCell(std::min(index, m_table->rowCount() - 1), kName);
    }

    RefreshEnabledState();
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::Clear()
{
    m_table->setRowCount(0);
    m_currentRow = -1;
    emit CurrentChanged(-1);
    RefreshEnabledState();
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::SetCanGenerate(bool possible)
{
    m_generate->setEnabled(possible);
    m_generate->setToolTip(possible ? tr("Lay these pieces out again")
                                    : tr("Import a PDF first"));
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::RefreshEnabledState()
{
    // « Export several outputs » is not wired yet, so it stays disabled whatever the list holds: a
    // button that comes alive and then does nothing when pressed is worse than one that says it is
    // not there yet.
    m_exportSeveral->setEnabled(false);
    m_exportSeveral->setToolTip(tr("Not available yet"));
}

//---------------------------------------------------------------------------------------------------------------------
void RunsDock::UpdateGlyphs()
{
    for (const Glyph &glyph : m_glyphs)
    {
        const bool lit = glyph.button->underMouse();
        glyph.button->setIcon(RabnestTheme::Icon(glyph.iconName, lit));
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool RunsDock::eventFilter(QObject *watched, QEvent *event)
{
    for (const Glyph &glyph : m_glyphs)
    {
        if (watched != glyph.button)
        {
            continue;
        }

        switch (event->type())
        {
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::EnabledChange:
            UpdateGlyphs();
            break;
        default:
            break;
        }
        break;
    }

    return QDockWidget::eventFilter(watched, event);
}
