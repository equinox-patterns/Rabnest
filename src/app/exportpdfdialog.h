/************************************************************************
 **
 **  @file   exportpdfdialog.h
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

#ifndef EXPORTPDFDIALOG_H
#define EXPORTPDFDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;

/// What a print run's file is called, and which folder it goes in.
///
/// **A print leaves as a PDF and nothing else**, so there is one question here that an image export
/// asks as a format: there is no format row, because there is nothing to choose. What is left is the
/// name — the run's own, which she may shorten — and the folder, which starts beside the PDF the run
/// was read from.
///
/// **The name is hers and the suffix is the code's.** A file dialogue lets a name keep a `.png` while
/// the file inside is something else, and the file she opens in her reader is then the wrong one.
/// Here the suffix is added when it is missing and never asked for.
class ExportPdfDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param fileName   the name proposed — the run's own, without its suffix
    /// @param directory  the folder proposed — the one the PDF was imported from
    ExportPdfDialog(const QString &fileName, const QString &directory, QWidget *parent = nullptr);

    auto directory() const -> QString;

    /// The name the file will have: what she typed, and `.pdf` when it is missing.
    auto fileName() const -> QString;

protected:
    /// Refuses a name or a folder that cannot be honoured.
    void accept() override;

private:
    QLineEdit *m_name = nullptr;
    QLineEdit *m_directory = nullptr;
};

#endif  // EXPORTPDFDIALOG_H
