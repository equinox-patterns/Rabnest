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
#include <QIcon>

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

    // The window and task bar icon, for Windows and Linux: without it a binary launched by hand has
    // the generic system icon.
    //
    // ⚠️ **NOT on macOS, and the old comment here was wrong.** `setWindowIcon()` does not get
    // ignored: Qt's Cocoa plugin calls `[NSApp setApplicationIconImage:]`, so the Dock tile becomes
    // the rasterised SVG — full-bleed, outside the bundle's `.icns` and outside Apple's icon grid,
    // which is why Rabnest looked bigger than every other app in the Dock. macOS reads the bundle's
    // `.icns` only when nothing sets an application icon, and that is what Ostara already does
    // (`#if !defined(Q_OS_MAC)` in its `main.cpp`).
#if !defined(Q_OS_MAC)
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app/rabnest.svg")));
#endif // !defined(Q_OS_MAC)

    RabnestWindow window;
    window.show();

    return QApplication::exec();
}
