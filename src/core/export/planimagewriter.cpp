/************************************************************************
 **
 **  @file   planimagewriter.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  A plan written as an image — the two syntaxes, each answering its own reader.
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

#include "core/export/planimagewriter.h"

#include <QBuffer>
#include <QDomDocument>
#include <QDomElement>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>
#include <QSvgGenerator>

#include <functional>

namespace
{
/// The resolution an SVG is written at. It is not a choice: the file declares its page in
/// millimetres from `size × 25.4 / resolution`, so this figure is what makes a plan of 140 cm be
/// 140 cm on the page. Ostara writes 96 here for the same reason.
constexpr int kSvgDpi = 96;

//---------------------------------------------------------------------------------------------------------------------
/// Every clip taken out of the document — they clip nothing, and they cost everything.
///
/// `QGraphicsScene::render` clips to the rectangle it renders and `QSvgGenerator` turns that into a
/// `<clipPath>` that nearly every `<g>` points at; a cartouche clips itself to its own box on top of
/// it. ⚠️ **Measured in Ostara, and this function exists for it:** Illustrator drops the opacity of
/// a document that carries a clip-path, so a faded text came out solid black there while Qt's own
/// reader and WebKit honoured both. Removing them changed ZERO pixels of the drawing — the clips
/// clip nothing.
///
/// A document that cannot be read back comes back untouched: an export must not fail over a nicety.
auto StripClips(const QByteArray &svg) -> QByteArray
{
    QDomDocument document;
    if (!document.setContent(svg))
    {
        return svg;
    }

    QVector<QDomElement> definitions;
    const QDomNodeList found = document.elementsByTagName(QStringLiteral("clipPath"));
    for (int index = 0; index < found.count(); ++index)
    {
        definitions.append(found.at(index).toElement());
    }

    if (definitions.isEmpty())
    {
        return svg;
    }

    // The references go first, by a walk rather than a lookup by tag name: what matters is the
    // attribute, not the element that happens to carry it.
    std::function<void(QDomElement)> strip = [&strip](QDomElement element) {
        element.removeAttribute(QStringLiteral("clip-path"));

        for (QDomElement child = element.firstChildElement(); !child.isNull();
             child = child.nextSiblingElement())
        {
            strip(child);
        }
    };
    strip(document.documentElement());

    for (const QDomElement &definition : definitions)
    {
        definition.parentNode().removeChild(definition);
    }

    return document.toByteArray(-1);
}

//---------------------------------------------------------------------------------------------------------------------
/// The opacity of every text moved off the text and onto a group around it.
///
/// ⚠️ **Measured on a real plan in Illustrator:** a group carrying `opacity` is honoured, a `<text>`
/// carrying only `fill-opacity` is not. So the text is given what the shapes already have — a group
/// of its own carrying the very figure its fill had, and an opaque fill. The result is EXACT: a
/// reader multiplied the group's opacity by the text's, and now multiplies the same two numbers.
auto LowerTextOpacity(const QByteArray &svg) -> QByteArray
{
    QDomDocument document;
    if (!document.setContent(svg))
    {
        return svg;
    }

    QVector<QDomElement> texts;
    const QDomNodeList found = document.elementsByTagName(QStringLiteral("text"));
    for (int index = 0; index < found.count(); ++index)
    {
        texts.append(found.at(index).toElement());
    }

    for (QDomElement text : texts) // by value: QDomElement is a handle, and setAttribute is not const
    {
        const QString figure = text.attribute(QStringLiteral("fill-opacity"));

        bool ok = false;
        const qreal opacity = figure.toDouble(&ok);
        if (!ok || opacity <= 0.0 || opacity >= 1.0)
        {
            continue; // a text drawn at full strength needs no wrapper, and gets none
        }

        QDomNode parent = text.parentNode();
        if (parent.isNull())
        {
            continue;
        }

        QDomElement wrapper = document.createElement(QStringLiteral("g"));
        wrapper.setAttribute(QStringLiteral("opacity"), figure); // the figure itself, not a rewrite
        text.setAttribute(QStringLiteral("fill-opacity"), QStringLiteral("1"));

        parent.replaceChild(wrapper, text); // the wrapper takes the text's place...
        wrapper.appendChild(text);          // ...and the text moves into it
    }

    return document.toByteArray(-1);
}

//---------------------------------------------------------------------------------------------------------------------
/// The plan as a PNG: white paper, the size in pixels, and the resolution written into the file.
auto WritePng(const QString &path, const PlanImageWriter::Renderer &render, const QRectF &sourcePx,
              const PlanImage::Target &target) -> bool
{
    QImage image(target.sizePx, QImage::Format_ARGB32);
    if (image.isNull())
    {
        return false;
    }

    // ⚠️ **White, and not transparent.** A plan is cloth on a table: a PNG with holes in it is a
    // plan that turns black in a viewer with a dark background, or that prints as whatever the page
    // behind it is. Ostara fills white for the same reason.
    image.fill(Qt::white);

    const int dotsPerMeter = static_cast<int>(std::lround(target.dpi / 25.4 * 1000.0));
    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);

    {
        QPainter painter(&image);
        if (!painter.isActive())
        {
            return false;
        }

        // The source is the plan's own rectangle and the target is the image, in pixels.
        render(&painter, sourcePx, QRectF(QPointF(0.0, 0.0), QSizeF(target.sizePx)));
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    QImageWriter writer(&file, QByteArrayLiteral("png"));
    if (!writer.write(image))
    {
        return false;
    }

    return file.commit();
}

//---------------------------------------------------------------------------------------------------------------------
/// The plan as an SVG whose page measures the size that was asked for.
auto WriteSvg(const QString &path, const PlanImageWriter::Renderer &render, const QRectF &sourcePx,
              const PlanImage::Target &target, const QString &title) -> bool
{
    // ⚠️ Never `setFileName`: the generator would open the file itself and report a failure with a
    // warning. Buffered, the write is ours and its failure is one boolean.
    QByteArray buffer;
    QBuffer device(&buffer);
    if (!device.open(QIODevice::WriteOnly))
    {
        return false;
    }

    // ⚠️ **SVG 1.1, and not the Tiny 1.2 Qt defaults to.** Tiny 1.2 restricts `opacity` to the
    // `image` element and says it must be treated as unsupported everywhere else — so a reader that
    // honours the declared profile drops every group opacity, which is exactly what the frame and
    // the assembly marks are made of.
    QSvgGenerator generator(QSvgGenerator::SvgVersion::Svg11);
    generator.setOutputDevice(&device);
    generator.setResolution(kSvgDpi);
    generator.setSize(target.sizePx);
    generator.setViewBox(sourcePx);
    if (!title.isEmpty())
    {
        generator.setTitle(title);
    }

    {
        QPainter painter(&generator);
        if (!painter.isActive())
        {
            return false;
        }

        // Source and target are the same rectangle: the painter's transform is then a translation of
        // the source's own origin, and the drawing comes out in the scene's pixels — which is
        // exactly what the viewBox declares. Scaling the painter as well would write the plan at
        // 1/96 of the size asked for.
        render(&painter, sourcePx, sourcePx);
    } // ~QPainter — it is the destructor that flushes the generator

    device.close();

    // Two passes over what the generator wrote, each answering a MEASURED defect of the format as
    // other software reads it.
    const QByteArray document = LowerTextOpacity(StripClips(buffer));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    if (file.write(document) != document.size())
    {
        return false;
    }

    return file.commit();
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
auto PlanImageWriter::Suffix(Format format) -> QString
{
    return format == Format::Png ? QStringLiteral("png") : QStringLiteral("svg");
}

//---------------------------------------------------------------------------------------------------------------------
auto PlanImageWriter::Write(const QString &path, const Renderer &render, const QRectF &sourcePx,
                            const PlanImage::Target &target, Format format, const QString &title)
    -> bool
{
    if (!render || path.isEmpty() || target.sizePx.isEmpty() || sourcePx.isEmpty())
    {
        return false;
    }

    return format == Format::Png ? WritePng(path, render, sourcePx, target)
                                 : WriteSvg(path, render, sourcePx, target, title);
}
