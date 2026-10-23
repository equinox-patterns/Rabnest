/************************************************************************
 **
 **  @file   rabnesttheme.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Ostara's look, applied to Rabnest.
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

#include "rabnesttheme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <QTextStream>

namespace
{
bool gDark = false;

/// Rabnest's copy of Ostara's tokens — the same values, in the same roles, because the two windows
/// are meant to look like one application. Fusion reads this palette for everything the stylesheet
/// does not reach: popup frames, tooltips, the scrollbar's own background.
void ApplyPalette(bool dark)
{
    QPalette palette;
    if (dark)
    {
        palette.setColor(QPalette::Window, QColor(0x23, 0x23, 0x23));
        palette.setColor(QPalette::WindowText, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::Base, QColor(0x19, 0x19, 0x19));
        palette.setColor(QPalette::AlternateBase, QColor(0x23, 0x23, 0x23));
        palette.setColor(QPalette::Text, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::Button, QColor(0x23, 0x23, 0x23));
        palette.setColor(QPalette::ButtonText, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::ToolTipBase, QColor(0x23, 0x23, 0x23));
        palette.setColor(QPalette::ToolTipText, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::Highlight, QColor(0x98, 0x3E, 0x60));
        palette.setColor(QPalette::HighlightedText, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::Light, QColor(0x4C, 0x4C, 0x4C));
        palette.setColor(QPalette::Midlight, QColor(0x33, 0x33, 0x33));
        palette.setColor(QPalette::Mid, QColor(0x33, 0x33, 0x33));
        palette.setColor(QPalette::Dark, QColor(0x19, 0x19, 0x19));
        palette.setColor(QPalette::Shadow, QColor(0x0C, 0x0C, 0x0C));
    }
    else
    {
        palette.setColor(QPalette::Window, QColor(0xF9, 0xF9, 0xF9));
        palette.setColor(QPalette::WindowText, QColor(0x2B, 0x2B, 0x2B));
        palette.setColor(QPalette::Base, QColor(0xF9, 0xF9, 0xF9));
        palette.setColor(QPalette::AlternateBase, QColor(0xF9, 0xF9, 0xF9));
        palette.setColor(QPalette::Text, QColor(0x2B, 0x2B, 0x2B));
        palette.setColor(QPalette::Button, QColor(0xF4, 0xF4, 0xF4));
        palette.setColor(QPalette::ButtonText, QColor(0x2B, 0x2B, 0x2B));
        palette.setColor(QPalette::ToolTipBase, QColor(0xF9, 0xF9, 0xF9));
        palette.setColor(QPalette::ToolTipText, QColor(0x2B, 0x2B, 0x2B));
        palette.setColor(QPalette::Highlight, QColor(0x8C, 0x45, 0x60));
        palette.setColor(QPalette::HighlightedText, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::Light, QColor(0xE5, 0xE5, 0xE5));
        palette.setColor(QPalette::Midlight, QColor(0xD6, 0xD6, 0xD6));
        palette.setColor(QPalette::Mid, QColor(0xD6, 0xD6, 0xD6));
        palette.setColor(QPalette::Dark, QColor(0x4C, 0x4C, 0x4C));
        palette.setColor(QPalette::Shadow, QColor(0x0C, 0x0C, 0x0C));
    }

    qApp->setPalette(palette);
}

/// The stylesheet, as the theme generator wrote it — read, never assembled here.
void ApplyStyleSheet(bool dark)
{
    QFile file(dark ? QStringLiteral(":/ostara/dark.qss") : QStringLiteral(":/ostara/light.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&file);
    qApp->setStyleSheet(stream.readAll());
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
void RabnestTheme::Apply(bool dark)
{
    gDark = dark;

    if (QStyle *style = QStyleFactory::create(QStringLiteral("fusion")); style != nullptr)
    {
        QApplication::setStyle(style);
    }

    // Loaded before the stylesheet, because the stylesheet names the family: a rule that asks for a
    // face nobody installed is silently dropped, and the window would fall back without saying so.
    if (int const id = QFontDatabase::addApplicationFont(
            QStringLiteral(":/fonts/inter/Inter-VariableFont.ttf"));
        id != -1)
    {
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
        {
            QFont font(families.first());
            font.setPixelSize(14);
            qApp->setFont(font);
        }
    }

    // The stylesheet first, then the palette — Ostara's order, and it matters: the stylesheet pushes
    // colours onto widgets as it polishes them, and a palette applied before it would be the one
    // those pushed colours mask. The window would then be right everywhere the sheet speaks and
    // wrong everywhere it does not.
    ApplyStyleSheet(dark);
    ApplyPalette(dark);
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestTheme::Icon(const QString &baseName) -> QIcon
{
    return Icon(baseName, gDark);
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestTheme::Icon(const QString &baseName, bool dark) -> QIcon
{
    const QString theme = dark ? QStringLiteral("dark") : QStringLiteral("light");
    return QIcon(QStringLiteral(":/icons/%1/%2_%1.svg").arg(theme, baseName));
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestTheme::InterfaceFontFamily() -> QString
{
    return qApp->font().family();
}
