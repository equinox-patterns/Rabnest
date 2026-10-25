/************************************************************************
 **
 **  @file   main.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/05/2026
 **
 **  @brief  Rabnest — reduce a pattern PDF, and produce what it becomes.
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

#include <QApplication>

#include "app/rabnestlanguage.h"
#include "app/rabnesttheme.h"
#include "app/rabnestwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Rabnest"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QApplication::setOrganizationName(QStringLiteral("Sophie Denys"));

    // **Before any widget exists, and after the application is named.** The name is what QSettings
    // files the choice under, so reading the language before it would never find the stored answer
    // and the interface would fall back to the system's on every launch.
    const QString language = RabnestLanguage::Current();
    if (RabnestLanguage::Load(language))
    {
        // One line, and it is there for one reason: a window that speaks the wrong language never
        // says why, and the console is where that answer costs nothing.
        qInfo("Language: %s", qPrintable(language));
    }
    else
    {
        qWarning("Language: %s — no translation file is embedded for it, the interface stays in English.",
                 qPrintable(language));
    }

    // Before any widget exists: the look is in place when the first one is built, so nothing is ever
    // drawn with a default it will not keep.
    RabnestTheme::Apply();

    RabnestWindow window;
    window.show();

    return QApplication::exec();
}
