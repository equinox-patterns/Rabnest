/************************************************************************
 **
 **  @file   licencedialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   26/10/2026
 **
 **  @brief  The licence texts, whole, in a window of their own.
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

#include "app/licencedialog.h"

#include <QFile>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace
{
/// The text as it travels in the binary. A missing file is said, never drawn as a crash: a licence
/// window that cannot open is worse than one that admits a packaging mistake.
auto Text(const QString &name) -> QString
{
    QFile file(QStringLiteral(":/licenses/%1").arg(name));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return LicenceDialog::tr("The licence text is missing from this build.");
    }

    return QString::fromUtf8(file.readAll());
}

/// A text, whole, in a block that wraps — and escaped, because a licence is prose and not markup.
auto Block(const QString &text) -> QString
{
    return QStringLiteral("<p style=\"font-size:11px; white-space:pre-wrap;\">%1</p>")
        .arg(text.toHtmlEscaped());
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
LicenceDialog::LicenceDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Licence texts"));
    setModal(true);
    resize(680, 640);
    setMinimumSize(520, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(8);

    m_body = new QTextBrowser(this);
    m_body->setOpenExternalLinks(true);
    m_body->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_body, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *close = new QPushButton(tr("Close"), this);
    close->setObjectName(QStringLiteral("primaryButton"));
    close->setCursor(Qt::PointingHandCursor);
    close->setDefault(true);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    // La licence du PROGRAMME d'abord : c'est celle qui engage Rabnest lui-même, et la seule que
    // la section 4 de la GPL exige de remettre avec chaque copie. Les composants viennent après,
    // dans l'ordre où ils apparaissent dans la fiche.
    m_body->setHtml(QStringLiteral("<h3>Rabnest — GNU General Public License 3</h3>%1"
                                  "<h3>Phosphor Icons — MIT</h3>%2"
                                  "<h3>Inter — SIL Open Font License 1.1</h3>%3")
                        .arg(Block(Text(QStringLiteral("GPL-3.0.txt"))),
                             Block(Text(QStringLiteral("phosphor-MIT.txt"))),
                             Block(Text(QStringLiteral("inter-OFL.txt")))));
    m_body->verticalScrollBar()->setValue(0);
}
