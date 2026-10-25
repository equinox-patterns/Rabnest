/************************************************************************
 **
 **  @file   checklistpage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  A step that asks one question: which of these do you keep?
 **
 **  @copyright
 **
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

#include "checklistpage.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

ChecklistPage::ChecklistPage(const QString &title, const QString &subtitle, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(8);

    m_title = new QLabel(title, this);
    m_title->setObjectName(QStringLiteral("stepTitle"));
    layout->addWidget(m_title);

    m_subtitle = new QLabel(subtitle, this);
    m_subtitle->setObjectName(QStringLiteral("stepSubtitle"));
    m_subtitle->setWordWrap(true);
    layout->addWidget(m_subtitle);
    layout->addSpacing(20);

    // The list grows with the pattern, so it scrolls rather than the window: a size range of
    // fourteen would otherwise push the buttons off a small screen, and the buttons are the only
    // way forward.
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_entries = new QWidget(scroll);
    m_entriesLayout = new QVBoxLayout(m_entries);
    m_entriesLayout->setContentsMargins(0, 0, 0, 0);
    m_entriesLayout->setSpacing(2);
    m_entriesLayout->addStretch();
    scroll->setWidget(m_entries);
    layout->addWidget(scroll, 1);

    m_empty = new QLabel(tr("Nothing to show."), this);
    m_empty->setObjectName(QStringLiteral("stepEmpty"));
    m_empty->setVisible(false);
    layout->addWidget(m_empty);

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 16, 0, 0);
    m_back = new QPushButton(tr("Back"), this);
    m_back->setObjectName(QStringLiteral("secondaryButton"));
    m_back->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_back);
    buttons->addStretch();
    m_continue = new QPushButton(tr("Next"), this);
    m_continue->setObjectName(QStringLiteral("primaryButton"));
    m_continue->setCursor(Qt::PointingHandCursor);
    m_continue->setDefault(true);
    buttons->addWidget(m_continue);
    layout->addLayout(buttons);

    connect(m_continue, &QPushButton::clicked, this, &ChecklistPage::Continue);
    connect(m_back, &QPushButton::clicked, this, &ChecklistPage::Back);
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::SetItems(const QVector<QPair<QString, QString>> &items)
{
    for (const QPair<QString, QCheckBox *> &entry : m_boxes)
    {
        m_entriesLayout->removeWidget(entry.second);
        entry.second->deleteLater();
    }
    m_boxes.clear();

    for (const QPair<QString, QString> &item : items)
    {
        auto *box = new QCheckBox(item.second, m_entries);
        box->setChecked(true);
        box->setCursor(Qt::PointingHandCursor);
        m_entriesLayout->insertWidget(m_entriesLayout->count() - 1, box);
        m_boxes.append({item.first, box});
        connect(box, &QCheckBox::toggled, this, &ChecklistPage::UpdateContinue);
    }

    m_empty->setVisible(items.isEmpty());
    UpdateContinue();
}

//---------------------------------------------------------------------------------------------------------------------
auto ChecklistPage::CheckedIds() const -> QStringList
{
    QStringList ids;
    ids.reserve(m_boxes.size());
    for (const QPair<QString, QCheckBox *> &entry : m_boxes)
    {
        if (entry.second->isChecked())
        {
            ids.append(entry.first);
        }
    }
    return ids;
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::SetCanContinue(bool canContinue)
{
    m_continue->setEnabled(canContinue);
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::SetContinueText(const QString &text)
{
    m_continue->setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::SetBackVisible(bool visible)
{
    m_back->setVisible(visible);
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::SetTexts(const QString &title, const QString &subtitle)
{
    m_title->setText(title);
    m_subtitle->setText(subtitle);
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
    }

    QWidget::changeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::Retranslate()
{
    // The title and the subtitle are NOT here: they are the caller's words, and it gives them again
    // through `SetTexts`. Writing them here would mean this page knowing whether it asks about sizes
    // or about pieces — and it deliberately does not.
    m_empty->setText(tr("Nothing to show."));
    m_back->setText(tr("Back"));
    m_continue->setText(tr("Next"));
}

//---------------------------------------------------------------------------------------------------------------------
void ChecklistPage::UpdateContinue()
{
    m_continue->setEnabled(!m_boxes.isEmpty() && !CheckedIds().isEmpty());
}
