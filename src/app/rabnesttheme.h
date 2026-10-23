/************************************************************************
 **
 **  @file   rabnesttheme.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Ostara's look, in Rabnest — the palette, the stylesheet, Inter, the icons.
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

#ifndef RABNESTTHEME_H
#define RABNESTTHEME_H

#include <QIcon>
#include <QString>

/// The application's single place for its appearance.
///
/// Rabnest is not Ostara and does not link Ostara: it carries a copy of the palette and of the
/// generated stylesheet, taken from `palette.json` and `template.qss.in`. That is deliberate — the
/// two applications must look like one hand drew them, and a shared library of styles for two
/// applications that never ship together would be a dependency bought with no benefit.
///
/// The icons are Ostara's own SVG set, copied whole with the same names and the same aliases, so a
/// stylesheet written for one finds its images in the other. Nothing is recoloured at runtime:
/// every file carries its own colour, and a theme is a different folder.
namespace RabnestTheme
{
/// Applies the palette, the stylesheet and the font. Called once, before the window exists.
///
/// @param dark  the dark palette and stylesheet instead of the light ones. Rabnest opens light:
///              it has no settings and no window to choose in, and a client who has never seen
///              the application should not have to discover a preference before using it.
void Apply(bool dark = false);

/// Ostara's icons: `:/icons/<theme>/<name>_<theme>.svg`.
///
/// One line, and it is the one place that knows how a themed icon is named — the same convention
/// the stylesheets use, so an icon handed to a button and an icon a rule draws are the same file.
auto Icon(const QString &baseName) -> QIcon;

/// The same icon in a NAMED theme, whatever the application's own is.
///
/// **The hover/selected rule needs it.** A row of the outputs dock is dark when it is hovered or
/// chosen — in both themes, because the accent colour is dark — so its icons must become the DARK
/// set at that moment. That is a second file in the same QIcon, and this is how it is named.
auto Icon(const QString &baseName, bool dark) -> QIcon;

/// The interface face, loaded from the application's own resources.
///
/// Rabnest ships Inter and installs it at startup: the client's machine has none of the faces this
/// application is drawn with, and a document that looks different depending on what is installed is
/// a document that has no design at all.
auto InterfaceFontFamily() -> QString;
}  // namespace RabnestTheme

#endif  // RABNESTTHEME_H
