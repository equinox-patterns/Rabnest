/************************************************************************
 **
 **  @file   importpage.h
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

#ifndef IMPORTPAGE_H
#define IMPORTPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;

/// The empty state: one sentence, one button, and the whole window behind it.
///
/// There is nothing to configure and nothing to remember — Rabnest keeps no document between two
/// openings — so the first screen has one job and says it once. A PDF dropped on the window is the
/// same gesture as the button, because dropping a file on the thing that wants a file is what
/// people do.
class ImportPage : public QWidget
{
    Q_OBJECT

public:
    explicit ImportPage(QWidget *parent = nullptr);

    /// Says why the file the user just picked was refused, under the button.
    void ShowError(const QString &message);
    void ClearError();

signals:
    void FileChosen(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    /// The interface has changed language: the three sentences of the card are set again. The word
    /// mark is a name, and a name does not move.
    void changeEvent(QEvent *event) override;

private:
    void Retranslate();

    QWidget *m_card = nullptr;
    QLabel *m_question = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_browse = nullptr;
    QLabel *m_error = nullptr;

    void Browse();
};

#endif  // IMPORTPAGE_H
