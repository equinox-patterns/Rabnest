/************************************************************************
 **
 **  @file   rabnestlanguage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   25/10/2026
 **
 **  @brief  The languages Rabnest speaks: the same seventeen as Ostara,
 **          the one it is on, and the way to change it.
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

#ifndef RABNESTLANGUAGE_H
#define RABNESTLANGUAGE_H

#include <QString>
#include <QVector>

/// The languages of the interface, and the only code that knows them.
///
/// **A language is a code and a label, never a file name spelled out at the call site.** The labels
/// are written here rather than asked to Qt, because Qt answers three of them badly: `es_ES` comes
/// back as « español de España », `id_ID` as « Indonesia », which is the country and not the
/// language, and the names change with every Qt release. One table, and the menu says the same thing
/// on every machine.
///
/// **The code stays the code.** `pt_BR` is the Brazilian file, and it is what pairs the application
/// with Qt's own Brazilian catalogue; only the label drops the country, because « português » is
/// what a reader of Portuguese calls their language.
///
/// English is the language the source is written in: it has no file and never needs one, so it is
/// listed last and loading it installs no translator at all.
namespace RabnestLanguage
{
struct Language
{
    QString code;
    QString label;
};

/// The seventeen translated languages, in code order, then English.
auto All() -> const QVector<Language> &;

/// The label of a code, or the code itself when it is not one of ours.
auto Label(const QString &code) -> QString;

/// The language the interface is in — the one she chose, and the system's until she chooses one.
auto Current() -> QString;

/// The system's language, when it is one we have; English otherwise.
///
/// A region is not a language: a machine set to `fr_CA` reads French, and answering English because
/// `fr_CA` is not in the list would be a worse answer than the one file we do have.
auto FromSystem() -> QString;

/// Stores the choice, so the next launch opens in it.
void Store(const QString &code);

/// Puts the interface in that language. `false` when the file is missing, which leaves the interface
/// in the language it was already in rather than blank.
auto Load(const QString &code) -> bool;
}  // namespace RabnestLanguage

#endif  // RABNESTLANGUAGE_H
