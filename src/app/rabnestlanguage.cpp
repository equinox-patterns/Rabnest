/************************************************************************
 **
 **  @file   rabnestlanguage.cpp
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

#include "app/rabnestlanguage.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace
{
/// English is the language the strings are written in. It is the one Qt starts in, so it is also the
/// one no file and no translator is needed for.
const QString kEnglish = QStringLiteral("en_US");

/// The setting, and the only one Rabnest keeps.
const QString kLanguageKey = QStringLiteral("language");

/// The translator in force, if any. It must outlive the call — a QTranslator destroyed while
/// installed leaves the interface pointing at freed dictionaries — so it is kept and swapped, never
/// left to a local.
QTranslator *g_translator = nullptr;
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
auto RabnestLanguage::All() -> const QVector<Language> &
{
    // The labels are the native names, with the orthography of their own language: most languages do
    // not capitalise their own name, and « Français » would be an English habit applied to a French
    // word.
    static const QVector<Language> languages{
        {QStringLiteral("cs_CZ"), QStringLiteral("čeština")},
        {QStringLiteral("de_DE"), QStringLiteral("Deutsch")},
        {QStringLiteral("el_GR"), QStringLiteral("Ελληνικά")},
        {QStringLiteral("es_ES"), QStringLiteral("español")},
        {QStringLiteral("fi_FI"), QStringLiteral("suomi")},
        {QStringLiteral("fr_FR"), QStringLiteral("français")},
        {QStringLiteral("hr_HR"), QStringLiteral("hrvatski")},
        {QStringLiteral("hu_HU"), QStringLiteral("magyar")},
        {QStringLiteral("id_ID"), QStringLiteral("bahasa Indonesia")},
        {QStringLiteral("it_IT"), QStringLiteral("italiano")},
        {QStringLiteral("ja_JP"), QStringLiteral("日本語")},
        {QStringLiteral("nl_NL"), QStringLiteral("Nederlands")},
        {QStringLiteral("pl_PL"), QStringLiteral("polski")},
        {QStringLiteral("pt_BR"), QStringLiteral("português")},
        {QStringLiteral("ro_RO"), QStringLiteral("română")},
        {QStringLiteral("uk_UA"), QStringLiteral("українська")},
        {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        {kEnglish, QStringLiteral("English")},
    };
    return languages;
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestLanguage::Label(const QString &code) -> QString
{
    for (const Language &language : All())
    {
        if (language.code == code)
        {
            return language.label;
        }
    }
    return code;
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestLanguage::FromSystem() -> QString
{
    const QLocale system = QLocale::system();

    // The exact one first: a machine set to pt_BR must not be answered pt_PT's file because the
    // language alone matches.
    for (const Language &language : All())
    {
        if (language.code == system.name())
        {
            return language.code;
        }
    }

    // Then the language, whatever its region. A machine set to fr_CA reads French, and the French
    // file is a better answer than English.
    for (const Language &language : All())
    {
        if (language.code != kEnglish && QLocale(language.code).language() == system.language())
        {
            return language.code;
        }
    }

    return kEnglish;
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestLanguage::Current() -> QString
{
    const QString stored = QSettings().value(kLanguageKey).toString();
    if (stored.isEmpty())
    {
        // Nothing has been chosen yet: the first launch speaks the system's language, if we have it.
        return FromSystem();
    }

    for (const Language &language : All())
    {
        if (language.code == stored)
        {
            return stored;
        }
    }

    // A code we no longer ship — a language dropped, or a settings file written by a future version.
    // Following the system is the only answer that cannot leave the interface in a language nobody
    // asked for.
    return FromSystem();
}

//---------------------------------------------------------------------------------------------------------------------
void RabnestLanguage::Store(const QString &code)
{
    QSettings().setValue(kLanguageKey, code);
}

//---------------------------------------------------------------------------------------------------------------------
auto RabnestLanguage::Load(const QString &code) -> bool
{
    if (g_translator != nullptr)
    {
        QCoreApplication::removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }

    if (code.isEmpty() || code == kEnglish)
    {
        return true;
    }

    auto *translator = new QTranslator(qApp);
    // The path `qt_add_translations` embeds its .qm files under, named after the target. The files
    // travel inside the binary: there is no folder to find, and nothing to install beside it.
    if (!translator->load(QStringLiteral(":/i18n/rabnest_%1.qm").arg(code)))
    {
        delete translator;
        return false;
    }

    g_translator = translator;
    QCoreApplication::installTranslator(g_translator);
    return true;
}
