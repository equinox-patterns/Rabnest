/************************************************************************
 **
 **  @file   aboutdialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   26/10/2026
 **
 **  @brief  What Rabnest is, who wrote it, what it owes, and the
 **          versions it is made of.
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

#include "app/aboutdialog.h"

#include "app/licencedialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace
{
/// A block of plain text, wrapped — the technical lines, and nothing else.
auto Block(const QString &text) -> QString
{
    return QStringLiteral("<p style=\"font-size:11px; white-space:pre-wrap;\">%1</p>")
        .arg(text.toHtmlEscaped());
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About Rabnest"));
    setModal(true);
    resize(640, 520);
    setMinimumSize(520, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 20);
    layout->setSpacing(6);

    // The word and nothing else, as on the home card: Rabnest has no mark, and a placeholder mark is
    // a decision nobody has made.
    m_wordmark = new QLabel(QStringLiteral("Rabnest"), this);
    m_wordmark->setObjectName(QStringLiteral("wordmark"));
    m_wordmark->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_wordmark);

    m_version = new QLabel(this);
    m_version->setObjectName(QStringLiteral("importHint"));
    m_version->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_version);

    // Le copyright du logiciel, tel qu'il est : un nom et une année qui ne se traduisent pas.
    m_copyright = new QLabel(QStringLiteral("© 2026 Sophie Denys"), this);
    m_copyright->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_copyright);

    m_legal = new QLabel(this);
    m_legal->setObjectName(QStringLiteral("importHint"));
    m_legal->setAlignment(Qt::AlignHCenter);
    m_legal->setWordWrap(true);
    layout->addWidget(m_legal);
    layout->addSpacing(12);

    m_body = new QTextBrowser(this);
    m_body->setOpenExternalLinks(true);
    m_body->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_body, 1);

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 8, 0, 0);

    m_copy = new QPushButton(tr("Copy the technical details"), this);
    m_copy->setObjectName(QStringLiteral("secondaryButton"));
    m_copy->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_copy);

    // Les deux textes obligatoires ne sont pas dans cette fiche : ils ont leur fenêtre, à un clic.
    m_licences = new QPushButton(tr("Licence texts"), this);
    m_licences->setObjectName(QStringLiteral("secondaryButton"));
    m_licences->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_licences);

    buttons->addStretch();

    m_close = new QPushButton(tr("Close"), this);
    m_close->setObjectName(QStringLiteral("primaryButton"));
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setDefault(true);
    buttons->addWidget(m_close);

    layout->addLayout(buttons);

    connect(m_copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(TechnicalText()); });
    connect(m_licences, &QPushButton::clicked, this, [this] {
        LicenceDialog dialog(this);
        dialog.exec();
    });
    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);

    Fill();
}

//---------------------------------------------------------------------------------------------------------------------
auto AboutDialog::TechnicalText() -> QString
{
    // Ce qui reste utile quand quelque chose ne va pas : la version de Rabnest, et le Qt qui
    // l'exécute. La révision, la date de compilation et le système ont été retirés — décision de
    // Sophie, 26-10-2026 : ils n'apprenaient rien qui ne se voie déjà sur la machine.
    return tr("Rabnest %1").arg(QCoreApplication::applicationVersion()) + QLatin1Char('\n')
           + tr("Qt: built with %1, running %2")
                 .arg(QString::fromLatin1(QT_VERSION_STR), QString::fromLatin1(qVersion()));
}

//---------------------------------------------------------------------------------------------------------------------
void AboutDialog::Fill()
{
    m_version->setText(tr("Version %1").arg(QCoreApplication::applicationVersion()));
    m_legal->setText(tr("A free program, published under the GNU General Public License, version 3 "
                        "or later. It comes with no warranty of any kind."));

    // What the application is built on, named in one line each: a reader who wants the terms has a
    // button for them, and a reader who does not is not made to scroll through four thousand words.
    const QString components =
        QStringLiteral("<p><b>%1</b> Phosphor Icons, MIT</p>"
                       "<p><b>%2</b> Inter, SIL Open Font License 1.1</p>"
                       "<p><b>%3</b> Clipper2, Boost Software License 1.0</p>"
                       "<p><b>%4</b> zlib</p>"
                       "<p><b>%5</b> Qt %6, LGPLv3</p>")
            .arg(tr("Icons:"), tr("Typeface:"), tr("Nesting:"), tr("Compression:"), tr("Framework:"),
                 QString::fromLatin1(QT_VERSION_STR));

    const QString source =
        QStringLiteral("<p><b>%1</b> <a href=\"https://github.com/equinox-patterns/Rabnest\">"
                       "https://github.com/equinox-patterns/Rabnest</a></p>")
            .arg(tr("Source code:"));

    const QString details = QStringLiteral("<h3>%1</h3>%2")
                                .arg(tr("Technical details"), Block(TechnicalText()));

    m_body->setHtml(components + source + details);
    m_body->verticalScrollBar()->setValue(0);
}
