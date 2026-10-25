/************************************************************************
 **
 **  @file   licencedialog.h
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

#ifndef LICENCEDIALOG_H
#define LICENCEDIALOG_H

#include <QDialog>

class QTextBrowser;

/// The two licence texts that have to travel with the application, shown whole.
///
/// **Their place is behind a button, not in the middle of « À propos ».** The MIT of the icons and
/// the SIL Open Font License of the typeface are four thousand characters of legal prose between
/// them; both must be included with the application, and neither has to be read by somebody who only
/// wanted to know which version of Qt is under the hood. So the About names them, this window shows
/// them, and the same text also sits as plain files in `Rabnest.app/Contents/Resources/licenses/`,
/// which is the form the Open Font License explicitly allows.
///
/// The text is read in the binary rather than from those files: a copy of the application whose
/// `Resources` folder was emptied must still be able to say what it owes.
class LicenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LicenceDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_body = nullptr;
};

#endif  // LICENCEDIALOG_H
