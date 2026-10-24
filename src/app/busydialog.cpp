/************************************************************************
 **
 **  @file   busydialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The dialogue a run shows while it works: what it is doing, and how long for.
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

#include "app/busydialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

//---------------------------------------------------------------------------------------------------------------------
BusyDialog::BusyDialog(QWidget *parent, const QString &title, const QString &text,
                       const QString &progressText)
  : QDialog(parent), m_text(text), m_progressText(progressText)
{
    setWindowTitle(title);
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    m_label = new QLabel(text, this);
    m_label->setWordWrap(true);
    layout->addWidget(m_label);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 0);
    m_bar->setTextVisible(false);
    layout->addWidget(m_bar);

    m_elapsed = new QLabel(QStringLiteral("0.0 s"), this);
    layout->addWidget(m_elapsed);

    setFixedWidth(400);

    m_clock.start();
    m_tick.setInterval(100);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        m_elapsed->setText(tr("%1 s").arg(m_clock.elapsed() / 1000.0, 0, 'f', 1));
    });
    m_tick.start();
}

//---------------------------------------------------------------------------------------------------------------------
void BusyDialog::SetProgress(int done, int total)
{
    if (total <= 0)
    {
        return;
    }

    m_bar->setRange(0, total);
    m_bar->setValue(done);

    // A run that names its steps says which one it is on; a run that only counts says nothing
    // more than the bar does, and the label keeps the sentence it opened with.
    if (!m_progressText.isEmpty())
    {
        m_label->setText(m_progressText.arg(done).arg(total));
    }
}
