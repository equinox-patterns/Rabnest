/************************************************************************
 **
 **  @file   layerspage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The whole journey for a PDF that is not Ostara's: keep the layers you want.
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

#ifndef LAYERSPAGE_H
#define LAYERSPAGE_H

#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
struct ImportedDocument;

/// A PDF from another application: there are no pieces to choose and nothing to re-nest.
///
/// What is left is real, and it is the reason Rabnest exists for these files at all: a plan
/// delivered with ten sizes as ten layers cannot be sent to a printer, because a printer prints the
/// file he is given. Ticking two of them and writing the file back is the whole offer — and the
/// layers are **removed** from the document, not switched off, because a hidden layer is one the
/// printer can switch back on.
///
/// The name and the folder are the only fields, and both are filled in: the file is written beside
/// the one it came from, under a name that says what it is. A client who changes nothing still gets
/// a correct file, which is what "one click" means.
class LayersPage : public QWidget
{
    Q_OBJECT

public:
    explicit LayersPage(QWidget *parent = nullptr);

    /// Fills the page from the document the user imported.
    void SetDocument(const ImportedDocument &document);

    /// The ids of the layers the user kept, in list order.
    QStringList CheckedIds() const;

    /// Says why the export the user just asked for did not happen.
    void ShowError(const QString &message);

signals:
    /// The user asked for the file. The path is what the name and the folder add up to.
    void Export(const QString &path);
    void Back();

protected:
    /// The interface has changed language: the page's own words are set again. The layer names are
    /// the PDF's, and those never move.
    void changeEvent(QEvent *event) override;

private:
    void Retranslate();

    void BrowseFolder();
    void ExportNow();
    void UpdateExportEnabled();

    QString m_folder;

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_folderLabel = nullptr;
    QPushButton *m_browse = nullptr;
    QPushButton *m_backButton = nullptr;

    QVBoxLayout *m_entriesLayout = nullptr;
    QLabel *m_empty = nullptr;
    QVector<QPair<QString, QCheckBox *>> m_boxes;  // layer id -> box, in list order

    QLineEdit *m_name = nullptr;
    QLineEdit *m_folderEdit = nullptr;
    QPushButton *m_export = nullptr;
    QLabel *m_error = nullptr;
};

#endif  // LAYERSPAGE_H
