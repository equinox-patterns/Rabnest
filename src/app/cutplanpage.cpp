/************************************************************************
 **
 **  @file   cutplanpage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Which sizes to lay out, and the cloth they will be laid on.
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

#include "app/cutplanpage.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include "app/print/printpanel.h"

namespace
{
auto CmSpin(qreal value, qreal maximum, QWidget *parent) -> QDoubleSpinBox *
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(0.0, maximum);
    spin->setDecimals(1);
    spin->setSingleStep(0.5);
    spin->setSuffix(QStringLiteral(" cm"));
    spin->setValue(value);
    return spin;
}

//---------------------------------------------------------------------------------------------------------------------
/// A tab that can scroll, because its height is not its own to decide.
///
/// ⚠️ **Without this the page is CRUSHED, not clipped.** The card the wizard lives in has a fixed
/// height — it is capped on every side so the workspace stays visible around it — and a cutting plan
/// asks for one line per size. Nine of them already ask for more than the card holds. A
/// `QVBoxLayout` short of room does not overflow: it takes the space from the items that have no
/// hard minimum, and a `QCheckBox` is one of them — its indicator is drawn 18 px high while the
/// widget's own minimum only knows the font. The result is a size list whose icons lose their top and
/// bottom edges, which reads as a corrupt checkbox rather than as a page that does not fit.
///
/// The viewport keeps a NATIVE WHITE background on macOS that no ordinary rule overrides; the dot
/// selector is the only thing that reaches it (see `Modifications/Ostara/SubtiliteCode`, fiche 02
/// §13). Both selectors carry the dot, so they paint what IS a scroll area or a bare widget, and
/// leave every subclass — the spin boxes, the labels, the checkboxes — alone.
auto Scrollable(QWidget *page, QWidget *parent) -> QScrollArea *
{
    auto *area = new QScrollArea(parent);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(page);

    area->setStyleSheet(QStringLiteral(".QScrollArea { background: transparent; }"));
    area->viewport()->setStyleSheet(QStringLiteral(".QWidget { background: transparent; }"));
    return area;
}

}  // namespace

//---------------------------------------------------------------------------------------------------------------------
CutPlanPage::CutPlanPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(8);

    // ── Ce que la génération fait, dit avant les onglets ────────────────────────────────────────
    auto *again = new QLabel(
        tr("Each generation makes one output. Once it is made, you can come back and make more."),
        this);
    again->setObjectName(QStringLiteral("panelHint"));
    again->setWordWrap(true);
    layout->addWidget(again);

    // ── Les trois onglets, comme dans Ostara ────────────────────────────────────────────────────
    //
    // One dialog, three ways of laying the same pattern out. The cutting plan is BUILT into its
    // tab, never moved into it: a page assembled first and reshuffled afterwards loses whatever is
    // not a widget — a QFormLayout and its rows, here — and the fields it held end up ownerless on
    // the page. That is exactly what happened, and it is why this reads as one pass.
    auto *tabs = new QTabWidget(this);

    auto *cutting = new QWidget(tabs);
    auto *cuttingLayout = new QVBoxLayout(cutting);
    cuttingLayout->setContentsMargins(16, 12, 16, 12);
    cuttingLayout->setSpacing(8);

    auto *title = new QLabel(tr("The cutting plan"), cutting);
    title->setObjectName(QStringLiteral("stepTitle"));
    cuttingLayout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("A cut plan is laid for one size. Tick the sizes you want: each one gets its own plan."),
        cutting);
    subtitle->setObjectName(QStringLiteral("stepSubtitle"));
    subtitle->setWordWrap(true);
    cuttingLayout->addWidget(subtitle);
    cuttingLayout->addSpacing(16);

    // ── Les tailles ─────────────────────────────────────────────────────────────────────────────
    m_boxesBlock = new QWidget(cutting);
    auto *boxesOuter = new QVBoxLayout(m_boxesBlock);
    boxesOuter->setContentsMargins(0, 0, 0, 0);
    boxesOuter->setSpacing(4);
    m_boxesLayout = new QVBoxLayout();
    m_boxesLayout->setContentsMargins(0, 0, 0, 0);
    m_boxesLayout->setSpacing(4);
    boxesOuter->addLayout(m_boxesLayout);
    cuttingLayout->addWidget(m_boxesBlock);
    cuttingLayout->addSpacing(12);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);

    m_width = CmSpin(140.0, 400.0, cutting);
    form->addRow(tr("Fabric width"), m_width);

    m_margin = CmSpin(0.5, 20.0, cutting);
    form->addRow(tr("Margin from the edges"), m_margin);

    m_gap = CmSpin(0.5, 20.0, cutting);
    form->addRow(tr("Gap between two pieces"), m_gap);

    cuttingLayout->addLayout(form);
    cuttingLayout->addSpacing(12);

    m_folded = new QCheckBox(tr("The fabric is folded"), cutting);
    m_folded->setCursor(Qt::PointingHandCursor);
    cuttingLayout->addWidget(m_folded);

    auto *foldedHint = new QLabel(
        tr("Folded: a piece cut on the fold is laid on the fold. Unfolded: that piece is shown "
           "whole, so unfolded. The width you enter is always the unfolded fabric."),
        cutting);
    foldedHint->setObjectName(QStringLiteral("outputDescription"));
    foldedHint->setWordWrap(true);
    foldedHint->setContentsMargins(24, 0, 0, 0);
    cuttingLayout->addWidget(foldedHint);

    m_oneWay = new QCheckBox(tr("All pieces the same way round"), cutting);
    m_oneWay->setCursor(Qt::PointingHandCursor);
    cuttingLayout->addWidget(m_oneWay);

    auto *oneWayHint = new QLabel(
        tr("The grain line is always respected, ticked or not. Unticked, pieces may be laid head to "
           "tail; ticked, they all lie the same way round."),
        cutting);
    oneWayHint->setObjectName(QStringLiteral("outputDescription"));
    oneWayHint->setWordWrap(true);
    oneWayHint->setContentsMargins(24, 0, 0, 0);
    cuttingLayout->addWidget(oneWayHint);

    // What does not fill the tab stays at its top, where it reads.
    cuttingLayout->addStretch(1);

    tabs->addTab(Scrollable(cutting, tabs), tr("Cutting plan"));

    // ── Impression ─────────────────────────────────────────────────────────────────────────────
    auto *printPage = new QWidget(tabs);
    auto *printLayout = new QVBoxLayout(printPage);
    printLayout->setContentsMargins(16, 12, 16, 12);
    printLayout->setSpacing(8);

    auto *printTitle = new QLabel(tr("The print"), printPage);
    printTitle->setObjectName(QStringLiteral("stepTitle"));
    printLayout->addWidget(printTitle);

    auto *printSubtitle = new QLabel(
        tr("The pattern is laid on the sheets you will print on, and the sheets carry their frame, "
           "their marks and their numbers."),
        printPage);
    printSubtitle->setObjectName(QStringLiteral("stepSubtitle"));
    printSubtitle->setWordWrap(true);
    printLayout->addWidget(printSubtitle);
    printLayout->addSpacing(12);

    m_print = new PrintPanel(printPage);
    printLayout->addWidget(m_print);
    printLayout->addStretch(1);

    tabs->addTab(Scrollable(printPage, tabs), tr("Print"));

    // ── Plotter ────────────────────────────────────────────────────────────────────────────────
    auto *plotter = new QWidget(tabs);
    auto *plotterLayout = new QVBoxLayout(plotter);
    auto *soon = new QLabel(tr("Coming soon"), plotter);
    soon->setAlignment(Qt::AlignCenter);
    plotterLayout->addStretch(1);
    plotterLayout->addWidget(soon);
    plotterLayout->addStretch(1);
    tabs->addTab(plotter, tr("Plotter"));

    m_tabs = tabs;
    layout->addWidget(tabs, 1);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("importError"));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);

    m_continue = new QPushButton(tr("Generate the plan"), this);
    m_continue->setObjectName(QStringLiteral("primaryButton"));
    m_continue->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_continue);

    connect(m_continue, &QPushButton::clicked, this, &CutPlanPage::Continue);

    auto *back = new QPushButton(tr("Back"), this);
    back->setObjectName(QStringLiteral("secondaryButton"));
    back->setCursor(Qt::PointingHandCursor);
    layout->addWidget(back);
    connect(back, &QPushButton::clicked, this, &CutPlanPage::Back);

    connect(m_folded, &QCheckBox::toggled, this, [this](bool folded) {
        // What she typed is the cloth as it comes off the bolt; the halving is the plan's business,
        // and the field must not change under her hands when she ticks the fold.
        m_width->setSuffix(folded ? tr(" cm (unfolded)") : tr(" cm"));
    });
}

//---------------------------------------------------------------------------------------------------------------------
void CutPlanPage::SetSizes(const QVector<QPair<QString, QString>> &sizes)
{
    for (const auto &entry : m_boxes)
    {
        delete entry.second;
    }
    m_boxes.clear();

    for (const auto &size : sizes)
    {
        auto *box = new QCheckBox(size.second, this);
        box->setChecked(true);
        box->setCursor(Qt::PointingHandCursor);
        m_boxesLayout->addWidget(box);
        m_boxes.append({size.first, box});
        connect(box, &QCheckBox::toggled, this, &CutPlanPage::UpdateContinue);
        connect(box, &QCheckBox::toggled, this, &CutPlanPage::UpdateGenerateText);
    }

    m_boxesBlock->setVisible(!sizes.isEmpty());
    UpdateGenerateText();
    UpdateContinue();
}

//---------------------------------------------------------------------------------------------------------------------
auto CutPlanPage::CheckedSizeIds() const -> QStringList
{
    QStringList ids;
    for (const auto &entry : m_boxes)
    {
        if (entry.second->isChecked())
        {
            ids.append(entry.first);
        }
    }
    return ids;
}

//---------------------------------------------------------------------------------------------------------------------
auto CutPlanPage::Request() const -> CutPlanRequest
{
    CutPlanRequest request;
    request.widthCm = m_width->value();
    request.marginCm = m_margin->value();
    request.gapCm = m_gap->value();
    request.foldedFabric = m_folded->isChecked();
    request.oneWay = m_oneWay->isChecked();
    return request;
}

//---------------------------------------------------------------------------------------------------------------------
void CutPlanPage::ShowError(const QString &message)
{
    m_error->setText(message);
    m_error->setVisible(!message.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void CutPlanPage::ClearError()
{
    m_error->clear();
    m_error->hide();
}

//---------------------------------------------------------------------------------------------------------------------
void CutPlanPage::UpdateContinue()
{
    // No size, no plan: there is no plan with nothing in it, so the way forward is closed rather
    // than leading to an empty canvas.
    m_continue->setEnabled(m_boxes.isEmpty() || !CheckedSizeIds().isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
auto CutPlanPage::ActiveTab() const -> Tab
{
    if (m_tabs == nullptr)
    {
        return Tab::Cutting;
    }

    switch (m_tabs->currentIndex())
    {
        case 1:
            return Tab::Print;
        case 2:
            return Tab::Plotter;
        default:
            return Tab::Cutting;
    }
}

//---------------------------------------------------------------------------------------------------------------------
auto CutPlanPage::PrintSettings() const -> PrintPanel *
{
    return m_print;
}

//---------------------------------------------------------------------------------------------------------------------
void CutPlanPage::UpdateGenerateText()
{
    if (m_print == nullptr)
    {
        return;
    }

    // What the first step kept, said where Ostara says it: the print panel's « Generate » line is a
    // reminder of the generation being made, never a choice.
    QStringList labels;
    for (const auto &entry : m_boxes)
    {
        if (entry.second->isChecked())
        {
            labels.append(entry.second->text());
        }
    }

    if (labels.isEmpty())
    {
        m_print->SetSizesText(QString());
    }
    else if (labels.size() == 1)
    {
        m_print->SetSizesText(tr("Size %1").arg(labels.first()));
    }
    else
    {
        m_print->SetSizesText(tr("Size range %1").arg(labels.join(QStringLiteral("–"))));
    }
}
