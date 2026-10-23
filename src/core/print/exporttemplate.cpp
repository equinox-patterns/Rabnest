/************************************************************************
 **
 **  @file   exporttemplate.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's template reader, ported.
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

#include "exporttemplate.h"

#include <QDomElement>
#include <QStringList>

namespace
{
// The attribute and element names of the recipe — the same strings Ostara writes
// (`oexporttemplate.cpp:45-80`). They are the contract between the two applications, so they are
// spelled once here and never interpolated.
const QString kTagFrame = QStringLiteral("frame");
const QString kTagMarks = QStringLiteral("marks");
const QString kTagPagination = QStringLiteral("pagination");

const QString kAttrWidthPt = QStringLiteral("widthPt");
const QString kAttrDashed = QStringLiteral("dashed");
const QString kAttrDashes = QStringLiteral("dashes");
const QString kAttrOpacity = QStringLiteral("opacity");
const QString kAttrScissorsHorizontal = QStringLiteral("scissorsHorizontal");
const QString kAttrScissorsVertical = QStringLiteral("scissorsVertical");
const QString kAttrScissorsOpacity = QStringLiteral("scissorsOpacity");

const QString kAttrShape = QStringLiteral("shape");
const QString kAttrSizeMm = QStringLiteral("sizeMm");
const QString kAttrCorner = QStringLiteral("corner");
const QString kAttrVerticalCentre = QStringLiteral("verticalCentre");
const QString kAttrHorizontalCentre = QStringLiteral("horizontalCentre");

const QString kAttrForm = QStringLiteral("form");
const QString kAttrEnabled = QStringLiteral("enabled");
const QString kAttrFontFamily = QStringLiteral("fontFamily");
const QString kAttrFontWeight = QStringLiteral("fontWeight");
const QString kAttrFontSize = QStringLiteral("fontSize");

const QString kTagCentre = QStringLiteral("centre");
const QString kTagCorner = QStringLiteral("corner");
const QString kTagVerticalCentre = QStringLiteral("verticalCentre");
const QString kTagHorizontalCentre = QStringLiteral("horizontalCentre");

auto ReadText(const QDomElement &element, const QString &name, const QString &fallback) -> QString
{
    return element.hasAttribute(name) ? element.attribute(name) : fallback;
}

auto ReadReal(const QDomElement &element, const QString &name, qreal fallback) -> qreal
{
    bool ok = false;
    const qreal value = element.attribute(name).toDouble(&ok);
    return ok ? value : fallback;
}

auto ReadInt(const QDomElement &element, const QString &name, int fallback) -> int
{
    bool ok = false;
    const int value = element.attribute(name).toInt(&ok);
    return ok ? value : fallback;
}

/// Ostara writes its flags as "1"/"0" (`oexporttemplate.cpp`), so that is what is read — and a
/// missing attribute keeps the default rather than reading as false.
auto ReadFlag(const QDomElement &element, const QString &name, bool fallback) -> bool
{
    if (!element.hasAttribute(name))
    {
        return fallback;
    }

    const QString value = element.attribute(name);
    return value == QLatin1String("1") || value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

/// The shape's token, as the recipe carries it: `circle`, `square`, `octagon`.
auto ShapeFromToken(const QString &token, ExportMarkShape fallback) -> ExportMarkShape
{
    const QVector<ExportMarkShapeModel> &shapes = ExportMarkShapes();
    for (int index = 0; index < shapes.size(); ++index)
    {
        if (token == QLatin1String(shapes.at(index).token))
        {
            return static_cast<ExportMarkShape>(index);
        }
    }
    return fallback;
}

/// The form's token, as the recipe carries it: `number`, `number+letter`.
auto FormFromToken(const QString &token, ExportPaginationForm fallback) -> ExportPaginationForm
{
    const QVector<ExportPaginationFormModel> &forms = ExportPaginationForms();
    for (int index = 0; index < forms.size(); ++index)
    {
        if (token == QLatin1String(forms.at(index).token))
        {
            return static_cast<ExportPaginationForm>(index);
        }
    }
    return fallback;
}

void ReadTextStyle(const QDomElement &element, ExportPaginationTextStyle &style)
{
    style.fontFamily = ReadText(element, kAttrFontFamily, style.fontFamily);
    // ⚠️ The attribute is named `fontWeight` and holds a FACE NAME, not a number — see
    // `ExportPaginationTextStyle::fontFace`. Ostara's own reader does the same
    // (`oexporttemplate.cpp:183-184`).
    style.fontFace = ReadText(element, kAttrFontWeight, style.fontFace);
    style.sizePx = ReadInt(element, kAttrFontSize, style.sizePx);
    style.opacity = ReadInt(element, kAttrOpacity, style.opacity);
}

void ReadPlacement(const QDomElement &parent, const QString &tag, ExportPaginationPlacement &out)
{
    const QDomElement element = parent.firstChildElement(tag);
    if (element.isNull())
    {
        return; // absent: the placement keeps the default it was given
    }

    out.enabled = ReadFlag(element, kAttrEnabled, out.enabled);
    ReadTextStyle(element, out.style);
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportTemplate::LoadFromXml(const QDomElement &root) -> bool
{
    if (root.isNull())
    {
        return false;
    }

    name = root.attribute(QStringLiteral("template"));

    const QDomElement frameElement = root.firstChildElement(kTagFrame);
    if (!frameElement.isNull())
    {
        frame.widthPt = ReadReal(frameElement, kAttrWidthPt, frame.widthPt);
        frame.dashed = ReadFlag(frameElement, kAttrDashed, frame.dashed);
        frame.frameOpacity = ReadInt(frameElement, kAttrOpacity, frame.frameOpacity);
        frame.scissorsHorizontal =
            ReadFlag(frameElement, kAttrScissorsHorizontal, frame.scissorsHorizontal);
        frame.scissorsVertical =
            ReadFlag(frameElement, kAttrScissorsVertical, frame.scissorsVertical);
        frame.scissorsOpacity = ReadInt(frameElement, kAttrScissorsOpacity, frame.scissorsOpacity);

        if (frameElement.hasAttribute(kAttrDashes))
        {
            QVector<qreal> pattern;
            const QStringList parts =
                frameElement.attribute(kAttrDashes).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &part : parts)
            {
                bool ok = false;
                const qreal value = part.toDouble(&ok);
                if (ok)
                {
                    pattern.append(value);
                }
            }
            if (!pattern.isEmpty())
            {
                frame.dashes = pattern;
            }
        }
    }

    const QDomElement marksElement = root.firstChildElement(kTagMarks);
    if (!marksElement.isNull())
    {
        marks.shape = ShapeFromToken(marksElement.attribute(kAttrShape), marks.shape);
        marks.sizeMm = ReadReal(marksElement, kAttrSizeMm, marks.sizeMm);
        marks.opacity = ReadInt(marksElement, kAttrOpacity, marks.opacity);
        marks.onCorner = ReadFlag(marksElement, kAttrCorner, marks.onCorner);
        marks.onVerticalCentre =
            ReadFlag(marksElement, kAttrVerticalCentre, marks.onVerticalCentre);
        marks.onHorizontalCentre =
            ReadFlag(marksElement, kAttrHorizontalCentre, marks.onHorizontalCentre);
    }

    const QDomElement paginationElement = root.firstChildElement(kTagPagination);
    if (!paginationElement.isNull())
    {
        pagination.form = FormFromToken(paginationElement.attribute(kAttrForm), pagination.form);
        ReadPlacement(paginationElement, kTagCentre, pagination.centre);
        ReadPlacement(paginationElement, kTagCorner, pagination.corner);
        ReadPlacement(paginationElement, kTagVerticalCentre, pagination.verticalCentre);
        ReadPlacement(paginationElement, kTagHorizontalCentre, pagination.horizontalCentre);
    }

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportTemplate::RabnestDefault() -> ExportTemplate
{
    // The decor Ostara's own shipped template draws, away from any file: the frame dashed at half
    // opacity, the octagon at forty millimetres, the number at the centre of each sheet and in its
    // corners. It is Rabnest's decision and it is stated here rather than in a file Rabnest does
    // not have.
    ExportTemplate decor;
    decor.name = QStringLiteral("Rabnest");

    decor.frame.dashed = true;
    decor.frame.dashes = {20.0, 10.0};
    decor.frame.frameOpacity = 50;
    decor.frame.scissorsHorizontal = true;
    decor.frame.scissorsVertical = true;
    decor.frame.scissorsOpacity = 50;

    decor.marks.shape = ExportMarkShape::Octagon;
    decor.marks.sizeMm = 40.0;
    decor.marks.opacity = 50;
    decor.marks.onCorner = true;

    decor.pagination.form = ExportPaginationForm::NumberAndLetter;

    // Inter, as everywhere a page number is written in Rabnest — the face both applications ship.
    const QString inter = QStringLiteral("Inter");
    decor.pagination.centre.style.fontFamily = inter;
    decor.pagination.corner.style.fontFamily = inter;
    decor.pagination.verticalCentre.style.fontFamily = inter;
    decor.pagination.horizontalCentre.style.fontFamily = inter;

    decor.pagination.centre.enabled = true;
    decor.pagination.centre.style.sizePx = 85;
    decor.pagination.centre.style.opacity = 25;

    decor.pagination.corner.enabled = true;
    decor.pagination.corner.style.sizePx = 16;
    decor.pagination.corner.style.opacity = 50;

    return decor;
}
