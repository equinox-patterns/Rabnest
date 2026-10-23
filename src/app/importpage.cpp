/************************************************************************
 **
 **  @file   importpage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The first thing Rabnest says: import a PDF.
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

#include "importpage.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
/// The file a drop carries, when it carries exactly one PDF. Anything else is ignored rather than
/// refused with a message: a drop that does nothing says "not that" well enough, and the button is
/// still there.
auto DroppedPdf(const QMimeData *mime) -> QString
{
    if (mime == nullptr || !mime->hasUrls())
    {
        return QString();
    }

    const QList<QUrl> urls = mime->urls();
    if (urls.size() != 1)
    {
        return QString();
    }

    const QString path = urls.first().toLocalFile();
    return path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive) ? path : QString();
}
}  // namespace

ImportPage::ImportPage(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 28, 32, 24);
    outer->addStretch();

    m_card = new QWidget(this);
    m_card->setObjectName(QStringLiteral("importCard"));
    m_card->setMaximumWidth(520);
    auto *card = new QVBoxLayout(m_card);
    card->setContentsMargins(28, 24, 28, 24);
    card->setSpacing(10);

    // The word and nothing else. Rabnest has no mark yet, and a placeholder mark is a decision
    // nobody has made: the name is enough to say where you are.
    auto *wordmark = new QLabel(QStringLiteral("Rabnest"), m_card);
    wordmark->setObjectName(QStringLiteral("wordmark"));
    wordmark->setAlignment(Qt::AlignHCenter);
    card->addWidget(wordmark);

    auto *question = new QLabel(tr("Import a PDF to begin."), m_card);
    question->setObjectName(QStringLiteral("importQuestion"));
    question->setAlignment(Qt::AlignHCenter);
    question->setWordWrap(true);
    card->addWidget(question);
    card->addSpacing(18);

    auto *browse = new QPushButton(tr("Import a PDF"), m_card);
    browse->setObjectName(QStringLiteral("primaryButton"));
    browse->setCursor(Qt::PointingHandCursor);
    browse->setDefault(true);
    card->addWidget(browse, 0, Qt::AlignHCenter);

    auto *hint = new QLabel(tr("or drop the file here"), m_card);
    hint->setObjectName(QStringLiteral("importHint"));
    hint->setAlignment(Qt::AlignHCenter);
    card->addWidget(hint);

    m_error = new QLabel(m_card);
    m_error->setObjectName(QStringLiteral("importError"));
    m_error->setAlignment(Qt::AlignHCenter);
    m_error->setWordWrap(true);
    m_error->setVisible(false);
    card->addWidget(m_error);

    auto *centre = new QHBoxLayout;
    centre->addStretch();
    centre->addWidget(m_card);
    centre->addStretch();
    outer->addLayout(centre);
    outer->addStretch();

    connect(browse, &QPushButton::clicked, this, &ImportPage::Browse);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportPage::ShowError(const QString &message)
{
    m_error->setText(message);
    m_error->setVisible(!message.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void ImportPage::ClearError()
{
    m_error->clear();
    m_error->setVisible(false);
}

//---------------------------------------------------------------------------------------------------------------------
void ImportPage::Browse()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import a PDF"), QString(),
                                                      tr("PDF (*.pdf)"));
    if (!path.isEmpty())
    {
        emit FileChosen(path);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void ImportPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (!DroppedPdf(event->mimeData()).isEmpty())
    {
        event->acceptProposedAction();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void ImportPage::dropEvent(QDropEvent *event)
{
    const QString path = DroppedPdf(event->mimeData());
    if (!path.isEmpty())
    {
        event->acceptProposedAction();
        emit FileChosen(path);
    }
}
