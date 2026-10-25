/************************************************************************
 **
 **  @file   layerspage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Keep the layers you want, and write the file back.
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

#include "layerspage.h"

#include "core/importeddocument.h"

#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

LayersPage::LayersPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(8);

    m_title = new QLabel(tr("Layers"), this);
    m_title->setObjectName(QStringLiteral("stepTitle"));
    layout->addWidget(m_title);

    m_subtitle = new QLabel(tr("This PDF does not come from Ostara: there are no pieces to choose and no plan "
                               "to lay out again. Tick the layers to keep — the others are "
                               "removed from the file."),
                           this);
    m_subtitle->setObjectName(QStringLiteral("stepSubtitle"));
    m_subtitle->setWordWrap(true);
    layout->addWidget(m_subtitle);
    layout->addSpacing(20);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    m_entriesLayout = new QVBoxLayout(content);
    m_entriesLayout->setContentsMargins(0, 0, 0, 0);
    m_entriesLayout->setSpacing(2);
    m_entriesLayout->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    m_empty = new QLabel(tr("This PDF carries no layer: there is nothing to reduce."), this);
    m_empty->setObjectName(QStringLiteral("stepEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setVisible(false);
    layout->addWidget(m_empty);

    // The file to write: a name, a folder, and nothing else. Both are filled in before the page is
    // shown, so a user who reads nothing and clicks the button still gets a correct file next to the
    // one she opened.
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 20, 0, 0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_name = new QLineEdit(this);
    form->addRow(tr("File name"), m_name);
    m_nameLabel = qobject_cast<QLabel *>(form->labelForField(m_name));

    auto *folderRow = new QWidget(this);
    auto *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(6);
    m_folderEdit = new QLineEdit(folderRow);
    m_folderEdit->setReadOnly(true);
    folderLayout->addWidget(m_folderEdit, 1);
    auto *browse = new QPushButton(tr("Choose…"), folderRow);
    browse->setObjectName(QStringLiteral("secondaryButton"));
    browse->setCursor(Qt::PointingHandCursor);
    folderLayout->addWidget(browse);
    form->addRow(tr("Folder"), folderRow);
    m_browse = browse;
    m_folderLabel = qobject_cast<QLabel *>(form->labelForField(folderRow));
    layout->addLayout(form);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("importError"));
    m_error->setWordWrap(true);
    m_error->setVisible(false);
    layout->addWidget(m_error);

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 16, 0, 0);
    auto *back = new QPushButton(tr("Back"), this);
    back->setObjectName(QStringLiteral("secondaryButton"));
    back->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(back);
    m_backButton = back;
    buttons->addStretch();
    m_export = new QPushButton(tr("Export the new PDF"), this);
    m_export->setObjectName(QStringLiteral("primaryButton"));
    m_export->setCursor(Qt::PointingHandCursor);
    m_export->setDefault(true);
    buttons->addWidget(m_export);
    layout->addLayout(buttons);

    connect(back, &QPushButton::clicked, this, &LayersPage::Back);
    connect(browse, &QPushButton::clicked, this, &LayersPage::BrowseFolder);
    connect(m_export, &QPushButton::clicked, this, &LayersPage::ExportNow);
}

//---------------------------------------------------------------------------------------------------------------------
void LayersPage::SetDocument(const ImportedDocument &document)
{
    for (const QPair<QString, QCheckBox *> &entry : m_boxes)
    {
        m_entriesLayout->removeWidget(entry.second);
        entry.second->deleteLater();
    }
    m_boxes.clear();

    for (const ImportedLayer &layer : document.layers())
    {
        auto *box = new QCheckBox(layer.label, this);
        box->setChecked(true);
        box->setCursor(Qt::PointingHandCursor);
        m_entriesLayout->insertWidget(m_entriesLayout->count() - 1, box);
        m_boxes.append({layer.id, box});
        connect(box, &QCheckBox::toggled, this, &LayersPage::UpdateExportEnabled);
    }

    m_empty->setVisible(document.layers().isEmpty());

    // The suffix says what the file is, not what was done to it: the same plan, fewer sizes. The
    // name is otherwise the one it came in with, full stop — a client who recognises her file is a
    // client who trusts the one she is about to send.
    m_name->setText(document.stem() + QStringLiteral("_Rabnest"));
    m_folder = QFileInfo(document.path()).absolutePath();
    m_folderEdit->setText(m_folder);

    m_error->clear();
    m_error->setVisible(false);
    UpdateExportEnabled();
}

//---------------------------------------------------------------------------------------------------------------------
auto LayersPage::CheckedIds() const -> QStringList
{
    QStringList ids;
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
void LayersPage::ShowError(const QString &message)
{
    m_error->setText(message);
    m_error->setVisible(!message.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void LayersPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
    }

    QWidget::changeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void LayersPage::Retranslate()
{
    m_title->setText(tr("Layers"));
    m_subtitle->setText(tr("This PDF does not come from Ostara: there are no pieces to choose and no plan "
                           "to lay out again. Tick the layers to keep — the others are "
                           "removed from the file."));
    m_empty->setText(tr("This PDF carries no layer: there is nothing to reduce."));

    // The two form labels are the only widgets of the page that were never held in a member: they are
    // made by the layout, so they are found back through it.
    if (m_nameLabel != nullptr)
    {
        m_nameLabel->setText(tr("File name"));
    }
    if (m_folderLabel != nullptr)
    {
        m_folderLabel->setText(tr("Folder"));
    }

    m_browse->setText(tr("Choose…"));
    m_backButton->setText(tr("Back"));
    m_export->setText(tr("Export the new PDF"));
}

//---------------------------------------------------------------------------------------------------------------------
void LayersPage::BrowseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Export folder"), m_folder);
    if (!folder.isEmpty())
    {
        m_folder = folder;
        m_folderEdit->setText(folder);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void LayersPage::ExportNow()
{
    QString name = m_name->text().trimmed();
    if (name.isEmpty())
    {
        ShowError(tr("Give the file a name."));
        return;
    }

    if (!name.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive))
    {
        name += QStringLiteral(".pdf");
    }

    m_error->clear();
    m_error->setVisible(false);
    emit Export(QDir(m_folder).filePath(name));
}

//---------------------------------------------------------------------------------------------------------------------
void LayersPage::UpdateExportEnabled()
{
    // Nothing ticked is the one answer that has no meaning, and keeping everything is the one that
    // achieves nothing: both close the way forward rather than writing a file that answers neither.
    int checked = 0;
    for (const QPair<QString, QCheckBox *> &entry : m_boxes)
    {
        if (entry.second->isChecked())
        {
            ++checked;
        }
    }

    m_export->setEnabled(checked > 0 && checked < m_boxes.size());
}
