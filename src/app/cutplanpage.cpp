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
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
CutPlanPage::CutPlanPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("The cutting plan"), this);
    title->setObjectName(QStringLiteral("stepTitle"));
    layout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("One plan per size: tick the sizes to lay out. The cloth below is the same for all of "
           "them — the grain, the fold and how many times a piece is cut come from the pattern, and "
           "Rabnest leaves them alone."),
        this);
    subtitle->setObjectName(QStringLiteral("stepSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);
    layout->addSpacing(16);

    // ── Les tailles ─────────────────────────────────────────────────────────────────────────────
    m_boxesBlock = new QWidget(this);
    auto *boxesOuter = new QVBoxLayout(m_boxesBlock);
    boxesOuter->setContentsMargins(0, 0, 0, 0);
    boxesOuter->setSpacing(4);
    m_boxesLayout = new QVBoxLayout();
    m_boxesLayout->setContentsMargins(0, 0, 0, 0);
    m_boxesLayout->setSpacing(4);
    boxesOuter->addLayout(m_boxesLayout);
    layout->addWidget(m_boxesBlock);
    layout->addSpacing(12);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);

    m_width = CmSpin(140.0, 400.0, this);
    form->addRow(tr("Fabric width"), m_width);

    m_margin = CmSpin(0.5, 20.0, this);
    form->addRow(tr("Margin from the edges"), m_margin);

    m_gap = CmSpin(0.5, 20.0, this);
    form->addRow(tr("Gap between two pieces"), m_gap);

    layout->addLayout(form);
    layout->addSpacing(12);

    m_folded = new QCheckBox(tr("The fabric is folded in two"), this);
    m_folded->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_folded);

    auto *foldedHint = new QLabel(
        tr("The width entered is the unfolded fabric: the plan is laid on half of it, and a piece "
           "cut on the fold is laid folded, its fold on the edge."),
        this);
    foldedHint->setObjectName(QStringLiteral("outputDescription"));
    foldedHint->setWordWrap(true);
    foldedHint->setContentsMargins(24, 0, 0, 0);
    layout->addWidget(foldedHint);

    m_oneWay = new QCheckBox(tr("All pieces the same way round"), this);
    m_oneWay->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_oneWay);

    auto *oneWayHint = new QLabel(
        tr("No piece is turned a half-turn. On a piece with two axes — the straight grain and the "
           "cross grain — both are still allowed, but each one way only."),
        this);
    oneWayHint->setObjectName(QStringLiteral("outputDescription"));
    oneWayHint->setWordWrap(true);
    oneWayHint->setContentsMargins(24, 0, 0, 0);
    layout->addWidget(oneWayHint);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("importError"));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);

    layout->addStretch(1);

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
    }

    m_boxesBlock->setVisible(!sizes.isEmpty());
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
