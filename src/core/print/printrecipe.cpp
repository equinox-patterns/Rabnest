/************************************************************************
 **
 **  @file   printrecipe.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Reads Ostara's `<exportRecipe>` out of the payload.
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

#include "printrecipe.h"

#include <QDomDocument>
#include <QDomElement>

namespace
{
const QString kTagMetadata = QStringLiteral("metadata");
const QString kTagRecipe = QStringLiteral("exportRecipe");
const QString kTagFormat = QStringLiteral("format");
const QString kTagDecor = QStringLiteral("decor");
const QString kTagCover = QStringLiteral("exportCover");

auto ReadReal(const QDomElement &element, const QString &name, qreal fallback) -> qreal
{
    bool ok = false;
    const qreal value = element.attribute(name).toDouble(&ok);
    return ok ? value : fallback;
}

auto ReadFlag(const QDomElement &element, const QString &name, bool fallback) -> bool
{
    if (!element.hasAttribute(name))
    {
        return fallback;
    }

    const QString value = element.attribute(name);
    return value == QLatin1String("1")
           || value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ReadPrintRecipe(const QByteArray &payload) -> PrintRecipe
{
    PrintRecipe recipe;

    if (payload.isEmpty())
    {
        return recipe;
    }

    QDomDocument document;
    if (!document.setContent(payload))
    {
        return recipe;
    }

    const QDomElement recipeElement =
        document.documentElement().firstChildElement(kTagMetadata).firstChildElement(kTagRecipe);
    if (recipeElement.isNull())
    {
        return recipe;
    }

    const QDomElement formatElement = recipeElement.firstChildElement(kTagFormat);
    if (!formatElement.isNull())
    {
        // The format line is what a run is reproduced from, so it is read even when the decor is
        // missing: a roll carries a format and no decor, and that is a complete statement.
        PrintFormat &format = recipe.format;
        format.present = true;
        format.token = formatElement.attribute(QStringLiteral("token"));
        format.framed = ReadFlag(formatElement, QStringLiteral("framed"), format.framed);
        format.columns = formatElement.attribute(QStringLiteral("columns")).toInt();
        if (format.columns < 1)
        {
            format.columns = 1;
        }
        format.documentWidthMm =
            ReadReal(formatElement, QStringLiteral("documentWidthMm"), format.documentWidthMm);
        format.frameHeightMm =
            ReadReal(formatElement, QStringLiteral("frameHeightMm"), format.frameHeightMm);
        format.marginMm = ReadReal(formatElement, QStringLiteral("marginMm"), format.marginMm);
        format.gapMm = ReadReal(formatElement, QStringLiteral("gapMm"), format.gapMm);
    }

    const QDomElement decorElement = recipeElement.firstChildElement(kTagDecor);
    if (!decorElement.isNull())
    {
        // A template that is written is a template that is drawn: `decor` present but empty is
        // still the creator's statement that this plan carries a decor, so it is read as one.
        recipe.hasDecor = recipe.decor.LoadFromXml(decorElement);
    }

    // **One substitution, decided with Sophie: the pagination's face.** The creator's font lives on
    // her machine and on nobody else's, so the numbers are drawn in Inter — the face both
    // applications ship — with the size, the weight and the opacity the recipe states. Nothing else
    // of the recipe changes.
    if (recipe.hasDecor)
    {
        const QString inter = QStringLiteral("Inter");
        recipe.decor.pagination.centre.style.fontFamily = inter;
        recipe.decor.pagination.corner.style.fontFamily = inter;
        recipe.decor.pagination.verticalCentre.style.fontFamily = inter;
        recipe.decor.pagination.horizontalCentre.style.fontFamily = inter;
    }

    const QDomElement coverElement =
        document.documentElement().firstChildElement(kTagMetadata).firstChildElement(kTagCover);
    if (!coverElement.isNull())
    {
        // `CarryCover` writes the cover page as ESCAPED text inside `<exportCover>`, so the DOM has
        // already unescaped it: the text is the SVG document itself.
        recipe.coverSvg = coverElement.text().toUtf8();
    }

    return recipe;
}
