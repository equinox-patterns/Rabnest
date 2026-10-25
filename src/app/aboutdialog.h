/************************************************************************
 **
 **  @file   aboutdialog.h
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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QTextBrowser;

/// « À propos » — the one place the application says what it is made of.
///
/// **Two of these licences must be shown, not only named.** The Phosphor icons are MIT, whose terms
/// are only kept by shipping the copyright line and the whole permission notice; the Inter typeface
/// is under the SIL Open Font License, which says the same in its own words. So their texts travel in
/// the binary (`/licenses/…`) and are printed whole, not summarised. The others — GPLv3 for Rabnest
/// itself, the LGPLv3 that Qt is used under, the Boost licence under Clipper2, zlib's — only need to
/// be named, and a summary is exactly what they get.
///
/// **Everything is read at the moment it is shown.** The dialogue is built when the action fires and
/// destroyed when it closes, so it always speaks the language in force and always tells the truth
/// about the binary it is running in.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

    /// The versions this binary is made of: its own, the revision it was built from, the date of the
    /// build, Qt built and running, and the system under it.
    ///
    /// Kept apart from the look because the « Copy » button hands exactly this text to the clipboard,
    /// and a bug report is much shorter to write when it starts with six lines nobody has to ask for.
    static auto TechnicalText() -> QString;

private:
    void Fill();

    QLabel *m_wordmark = nullptr;
    QLabel *m_version = nullptr;
    QLabel *m_copyright = nullptr;
    QLabel *m_legal = nullptr;
    QTextBrowser *m_body = nullptr;
    QPushButton *m_copy = nullptr;
    QPushButton *m_licences = nullptr;
    QPushButton *m_close = nullptr;
};

#endif  // ABOUTDIALOG_H
