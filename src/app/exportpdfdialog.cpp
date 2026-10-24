/************************************************************************
 **
 **  @file   exportpdfdialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   24/10/2026
 **
 **  @brief  Where a print run goes, and what the file is called.
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

#include "app/exportpdfdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

//---------------------------------------------------------------------------------------------------------------------
ExportPdfDialog::ExportPdfDialog(const QString &fileName, const QString &directory, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export the print as PDF"));

    // **A name worth reading is a name worth showing whole.** The proposed name states the sizes and
    // the paper — « Taille 32 to Taille 36 - A0 Roll » — and a field narrower than it would open with
    // the end of it cut off, on the one line she is here to read.
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // ── Le nom ─────────────────────────────────────────────────────────────────────────────────
    //
    // The run's own name, editable: it says what the run covers — its sizes and its format — and it
    // is long because it is complete. Shortening it is her decision, and the file keeps whatever she
    // leaves.
    m_name = new QLineEdit(fileName, this);
    form->addRow(tr("File name"), m_name);

    // ── Le dossier ─────────────────────────────────────────────────────────────────────────────
    //
    // The PDF the run was read from has a folder, and that is where its print belongs: a delivery
    // that starts a machine's Documents folder would be a delivery she has to hunt for. Typed or
    // browsed, and never guessed.
    const QString start = directory.isEmpty()
                              ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                              : directory;

    auto *folderRow = new QWidget(this);
    auto *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    m_directory = new QLineEdit(start, folderRow);
    folderLayout->addWidget(m_directory, 1);
    auto *browse = new QPushButton(tr("Browse..."), folderRow);
    browse->setCursor(Qt::PointingHandCursor);
    folderLayout->addWidget(browse);
    form->addRow(tr("Folder"), folderRow);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString chosen =
            QFileDialog::getExistingDirectory(this, tr("Folder"), m_directory->text());
        if (!chosen.isEmpty())
        {
            m_directory->setText(chosen);
        }
    });

    layout->addLayout(form);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    auto *exportButton = buttons->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setObjectName(QStringLiteral("primaryButton"));
    exportButton->setCursor(Qt::PointingHandCursor);
    auto *cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setCursor(Qt::PointingHandCursor);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ExportPdfDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ExportPdfDialog::reject);
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportPdfDialog::directory() const -> QString
{
    return m_directory->text().trimmed();
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportPdfDialog::fileName() const -> QString
{
    const QString typed = m_name->text().trimmed();
    return typed.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive) ? typed
                                                                     : typed + QStringLiteral(".pdf");
}

//---------------------------------------------------------------------------------------------------------------------
void ExportPdfDialog::accept()
{
    if (m_name->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, tr("Export"), tr("The file needs a name."));
        return;
    }

    if (!QDir(directory()).exists())
    {
        QMessageBox::warning(this, tr("Export"), tr("That folder does not exist."));
        return;
    }

    QDialog::accept();
}
