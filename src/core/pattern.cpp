/************************************************************************
 **
 **  @file   pattern.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The pieces of an Ostara PDF, read as a cutting plan reads them.
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

#include "core/pattern.h"

#include "core/importeddocument.h"

#include <QDomDocument>
#include <QDomElement>
#include <QXmlStreamWriter>
#include <QObject>
#include <QHash>
#include <QLineF>
#include <QSet>

#include <algorithm>
#include <QRegularExpression>

#include <cmath>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
auto Numbers(const QString &text) -> QVector<double>
{
    QVector<double> values;
    QString current;

    for (const QChar character : text)
    {
        if (character.isDigit() || character == QLatin1Char('.') || character == QLatin1Char('-')
            || character == QLatin1Char('+'))
        {
            current.append(character);
        }
        else
        {
            if (!current.isEmpty())
            {
                values.append(current.toDouble());
                current.clear();
            }
        }
    }
    if (!current.isEmpty())
    {
        values.append(current.toDouble());
    }

    return values;
}

//---------------------------------------------------------------------------------------------------------------------
auto Outline(const QString &text) -> QVector<QPointF>
{
    const QVector<double> values = Numbers(text);

    QVector<QPointF> points;
    points.reserve(values.size() / 2);
    for (int index = 0; index + 1 < values.size(); index += 2)
    {
        points.append(QPointF(values.at(index), values.at(index + 1)));
    }
    return points;
}

//---------------------------------------------------------------------------------------------------------------------
/// The contour as DRAWN, one polygon per subpath — `M x,y` … `L x,y` … repeated.
///
/// Ostara writes it with the same serializer as the drawing's own `<path d>` (`M`/`L`, three
/// decimals, no `Z`), and splits the subpaths with `M`. Nothing here interprets a curve: a piece's
/// contour is polygons all the way down — the sewing line, the seam allowance, and the notches
/// spliced into one of them. A `C` would be read as more of the same polyline, which is what a
/// cutting plan's own outline reader has always done.
auto InkPaths(const QString &text) -> QVector<QPolygonF>
{
    QVector<QPolygonF> paths;

    for (const QString &chunk : text.split(QLatin1Char('M'), Qt::SkipEmptyParts))
    {
        const QVector<double> values = Numbers(chunk);
        if (values.size() < 6)
        {
            continue;
        }

        QPolygonF polygon;
        polygon.reserve(values.size() / 2);
        for (int index = 0; index + 1 < values.size(); index += 2)
        {
            polygon.append(QPointF(values.at(index), values.at(index + 1)));
        }
        paths.append(polygon);
    }

    return paths;
}

//---------------------------------------------------------------------------------------------------------------------
auto Shifted(const QVector<QPointF> &points, const QPointF &delta) -> QVector<QPointF>
{
    QVector<QPointF> out;
    out.reserve(points.size());
    for (const QPointF &point : points)
    {
        out.append(point + delta);
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
auto Shifted(const QVector<QPolygonF> &paths, const QPointF &delta) -> QVector<QPolygonF>
{
    QVector<QPolygonF> out;
    out.reserve(paths.size());
    for (const QPolygonF &path : paths)
    {
        out.append(path.translated(delta));
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
auto Mirrored(const QVector<QPolygonF> &paths, double axisX) -> QVector<QPolygonF>
{
    QVector<QPolygonF> out;
    out.reserve(paths.size());
    for (const QPolygonF &path : paths)
    {
        QPolygonF one;
        one.reserve(path.size());
        for (const QPointF &point : path)
        {
            one.append(QPointF(2.0 * axisX - point.x(), point.y()));
        }
        out.append(one);
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
/// The name the DRAWING gives a piece — Ostara's own `OSvg::SanitizeSvgId`, and not a rule of ours.
///
/// ⚠️ **« Replace a space with an underscore » is not it, and the difference loses pieces.** The
/// drawing keeps letters, digits, `_` and `-` and turns EVERY other character into an underscore, so
/// « Poignet x2 + entoilage » is drawn `Poignet_x2___entoilage` — three underscores, one for the
/// space, one for the `+`, one for the space after it. Compared on the space alone, that piece
/// matched nothing and was dropped from the plan without a word.
auto Slug(const QString &name) -> QString
{
    QString slug;
    slug.reserve(name.size());
    for (const QChar character : name)
    {
        slug += (character.isLetterOrNumber() || character == QLatin1Char('_')
                 || character == QLatin1Char('-'))
                    ? character
                    : QLatin1Char('_');
    }
    if (!slug.isEmpty() && slug.front().isDigit())
    {
        slug.prepend(QLatin1Char('_'));
    }
    return slug;
}

//---------------------------------------------------------------------------------------------------------------------
auto CentreX(const QVector<QPointF> &points) -> double
{
    if (points.isEmpty())
    {
        return 0.0;
    }

    double low = points.first().x();
    double high = low;
    for (const QPointF &point : points)
    {
        low = std::min(low, point.x());
        high = std::max(high, point.x());
    }
    return (low + high) / 2.0;
}

//---------------------------------------------------------------------------------------------------------------------
auto Mirrored(const QVector<QPointF> &points, double axisX) -> QVector<QPointF>
{
    QVector<QPointF> out;
    out.reserve(points.size());
    for (const QPointF &point : points)
    {
        out.append(QPointF(2.0 * axisX - point.x(), point.y()));
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
/// A reflection about a vertical axis sends an angle `a` to `180 − a` — the same rule the direction
/// of a piece follows, and it must be applied here too: the mirror lives in the outline, and the
/// angle is read on the piece, which the outline says nothing about.
auto MirroredGrain(double angle) -> double
{
    double out = std::fmod(180.0 - angle, 360.0);
    if (out < 0.0)
    {
        out += 360.0;
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
/// An affine matrix, in Qt's own order: `x' = a·x + c·y + e`, `y' = b·x + d·y + f`.
struct Matrix
{
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double e = 0.0;
    double f = 0.0;
};

auto Compose(const Matrix &outer, const Matrix &inner) -> Matrix
{
    Matrix out;
    out.a = outer.a * inner.a + outer.c * inner.b;
    out.b = outer.b * inner.a + outer.d * inner.b;
    out.c = outer.a * inner.c + outer.c * inner.d;
    out.d = outer.b * inner.c + outer.d * inner.d;
    out.e = outer.a * inner.e + outer.c * inner.f + outer.e;
    out.f = outer.b * inner.e + outer.d * inner.f + outer.f;
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
/// The matrix an SVG `transform` attribute states. `matrix`, `translate` and `scale` are what the
/// writer emits; anything else is ignored rather than guessed.
auto ParseTransform(const QString &text) -> Matrix
{
    Matrix matrix;

    const QRegularExpression pattern(
        QStringLiteral("(matrix|translate|scale)\\s*\\(([^)]*)\\)"));
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const QVector<double> values = Numbers(match.captured(2));

        Matrix step;
        if (match.captured(1) == QLatin1String("matrix") && values.size() >= 6)
        {
            step.a = values.at(0);
            step.b = values.at(1);
            step.c = values.at(2);
            step.d = values.at(3);
            step.e = values.at(4);
            step.f = values.at(5);
        }
        else if (match.captured(1) == QLatin1String("translate") && !values.isEmpty())
        {
            step.e = values.at(0);
            step.f = values.size() > 1 ? values.at(1) : 0.0;
        }
        else if (match.captured(1) == QLatin1String("scale") && !values.isEmpty())
        {
            step.a = values.at(0);
            step.d = values.size() > 1 ? values.at(1) : values.at(0);
        }

        matrix = Compose(matrix, step);
    }

    return matrix;
}



//---------------------------------------------------------------------------------------------------------------------
/// A box around everything a subtree draws, with its transforms applied.
///
/// **Crude, and on purpose.** The numbers of a path include its control points, which is a box larger
/// than the ink — and a box that is too large only costs a little empty space in an SVG viewBox, while
/// one that is too small cuts a cartouche in half.
struct GrowingBox
{
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    bool empty = true;

    /// ⚠️ **`QRectF::united()` cannot do this.** It ignores a null rectangle, and a point is a null
    /// rectangle — so unioning points leaves zero width and the box stays empty for ever. The earlier
    /// canvas crop failed on exactly this, and it failed silently both times.
    void Add(double x, double y)
    {
        if (empty)
        {
            x0 = x1 = x;
            y0 = y1 = y;
            empty = false;
            return;
        }
        x0 = std::min(x0, x);
        x1 = std::max(x1, x);
        y0 = std::min(y0, y);
        y1 = std::max(y1, y);
    }

    auto Rect() const -> QRectF
    {
        return empty ? QRectF() : QRectF(x0, y0, x1 - x0, y1 - y0);
    }
};

auto DecorBox(const QDomElement &element, const Matrix &outer, GrowingBox *box) -> void
{
    const Matrix matrix = Compose(outer, ParseTransform(element.attribute(QStringLiteral("transform"))));

    // ⚠️ **A `<text>` has no `d`, and the box used to ignore it entirely.** The fold mark's caption is
    // written as editable text — a `<text>` with its own `x`/`y` and a placement — so it contributed
    // nothing to the measured box, and the viewBox built from that box clipped it: the caption fell
    // outside the drawing, sometimes outside the canvas altogether. Measured here from its anchor and
    // its font size, generously: a caption is a few words, and a box that is a little too large shows
    // a little white, where one that is too small loses the text.
    if (element.tagName() == QLatin1String("text"))
    {
        const double x = element.attribute(QStringLiteral("x")).toDouble();
        const double y = element.attribute(QStringLiteral("y")).toDouble();
        double size = element.attribute(QStringLiteral("font-size")).toDouble();
        if (size <= 0.0)
        {
            size = 10.0;
        }
        const double width = size * 0.6 * static_cast<double>(element.text().size() + 1);

        const double mappedX = matrix.a * x + matrix.c * y + matrix.e;
        const double mappedY = matrix.b * x + matrix.d * y + matrix.f;
        const double endX = matrix.a * (x + width) + matrix.c * (y + size) + matrix.e;
        const double endY = matrix.b * (x + width) + matrix.d * (y + size) + matrix.f;

        // The text is drawn along its own baseline, above it: one font size of headroom on both sides
        // covers the ascent and the descender without measuring the face.
        box->Add(std::min(mappedX, endX) - size, std::min(mappedY, endY) - size);
        box->Add(std::max(mappedX, endX) + size, std::max(mappedY, endY) + size);
    }

    const QString data = element.attribute(QStringLiteral("d"));
    if (!data.isEmpty())
    {
        const QVector<double> values = Numbers(data);
        for (int index = 0; index + 1 < values.size(); index += 2)
        {
            const double x = values.at(index);
            const double y = values.at(index + 1);

            const double mappedX = matrix.a * x + matrix.c * y + matrix.e;
            const double mappedY = matrix.b * x + matrix.d * y + matrix.f;

            box->Add(mappedX, mappedY);
        }
    }

    for (QDomElement child = element.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        DecorBox(child, matrix, box);
    }
}

//---------------------------------------------------------------------------------------------------------------------
auto WriteNode(QXmlStreamWriter &writer, const QDomNode &node) -> void
{
    if (node.isElement())
    {
        const QDomElement element = node.toElement();
        writer.writeStartElement(element.tagName());

        const QDomNamedNodeMap attributes = element.attributes();
        for (int index = 0; index < attributes.count(); ++index)
        {
            const QDomAttr attribute = attributes.item(index).toAttr();
            if (!attribute.isNull()
                && !attribute.name().startsWith(QLatin1String("xmlns")))
            {
                writer.writeAttribute(attribute.name(), attribute.value());
            }
        }

        for (QDomNode child = element.firstChild(); !child.isNull(); child = child.nextSibling())
        {
            if (child.isText())
            {
                // ⚠️ **The text of a node is what a `<style>` element IS.** Skipping text nodes
                // wrote every stylesheet empty, and the cartouche's paths — which take their colours
                // from `.st0` / `.st1` classes — fell back to the default fill. That is the black
                // blob where the logo should be.
                writer.writeCharacters(child.toText().data());
                continue;
            }

            WriteNode(writer, child);
        }

        writer.writeEndElement();
    }
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
/// One owner's `<decor>` from the payload, ready to be placed.
///
/// The subtree is kept as it was written and its box measured in the SAME frame — no matrix is
/// undone, because no matrix is applied: Ostara writes the ink in the owner's own frame, the one its
/// outline travels in. An empty or degenerate block is reported as absent, never as an empty drawing
/// that would then be placed and drawn nowhere.
auto ReadDecor(const QDomElement &decorElement, PieceDecor *out) -> bool
{
    if (out == nullptr || decorElement.isNull())
    {
        return false;
    }

    QString content;
    QXmlStreamWriter writer(&content);
    GrowingBox box;

    for (QDomElement child = decorElement.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        WriteNode(writer, child);
        DecorBox(child, Matrix{}, &box);
    }

    if (content.isEmpty() || box.Rect().isNull())
    {
        return false;
    }

    out->content = content;
    out->box = box.Rect();
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
auto Pattern::Load(const ImportedDocument &document, const QStringList &sizeIds,
                   const QStringList &pieceNames, Cloth cloth, QVector<CutSize> &out,
                   QString *error) -> bool
{
    out.clear();

    QDomDocument payload;
    if (!payload.setContent(document.payload()))
    {
        if (error != nullptr)
        {
            *error = QObject::tr("The pattern carried by this PDF cannot be read.");
        }
        return false;
    }

    const QDomElement metadata =
        payload.documentElement().firstChildElement(QStringLiteral("metadata"));
    const QDomElement pieces = metadata.firstChildElement(QStringLiteral("pieces"));
    if (pieces.isNull())
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF was written before Rabnest could read a cutting plan from "
                                 "it: it carries no pieces.");
        }
        return false;
    }

    // **Version 3 is the one that carries the drawn contour and the alignment.** Version 2 holds
    // the same pieces with their decor, but a reader can only lay their sizes on top of each other
    // by guessing an offset it never received — which gives a footprint larger than the piece, not
    // a slightly wrong one — and can only draw them without their notches. Version 1 has no decor
    // at all. Reading the nearest older thing would be worse than refusing it: the plan would lay
    // ten pieces and not one grain line, silently. A reader that cannot find what it was asked for
    // says so.
    const QString payloadVersion = pieces.attribute(QStringLiteral("version"));
    if (payloadVersion != QLatin1String("3"))
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF was exported by an older Ostara: it does not carry the "
                                 "drawn contour of its pieces, nor the offset that lines their sizes "
                                 "up. Export it again.");
        }
        return false;
    }

    QHash<QString, QString> labelById;
    for (const ImportedLayer &layer : document.layers())
    {
        labelById.insert(layer.id, layer.label);
    }

    const QSet<QString> kept(pieceNames.cbegin(), pieceNames.cend());

    // ── Le décor ────────────────────────────────────────────────────────────────────────────────
    //
    // Read from the PAYLOAD, and never from the drawing again. Ostara copies each owner's children
    // into EVERY size's record — the same ink, the same coordinates, resolved once — so a plan of
    // size 40 finds the children of 40 where they were drawn, in the piece's own frame, and places
    // them with the piece. Nothing is measured, shifted or anchored here: the payload states a
    // position, and Rabnest uses it.
    //
    // A grouped member carries none of its own — Graded and Details both hand its decor to the group
    // — so the group's block is read first and stands in for every member that lacks one.
    QHash<QString, PieceDecor> groupDecor;
    for (QDomElement group = pieces.firstChildElement(QStringLiteral("group")); !group.isNull();
         group = group.nextSiblingElement(QStringLiteral("group")))
    {
        PieceDecor one;
        if (ReadDecor(group.firstChildElement(QStringLiteral("decor")), &one))
        {
            groupDecor.insert(group.attribute(QStringLiteral("id")), one);
        }
    }

    // One plan per size, as Ostara does it: a cutting plan is laid for one size, and asking for
    // three gives three plans, not one plan over three sizes.
    // The reference size — the first the payload writes, which is the smallest of the range, and
    // the only one whose decor the drawing resolved. Read here, before anything is filtered out: a
    // piece the run does not keep still names its layer.
    QString referenceLabel;
    for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece")); !element.isNull();
         element = element.nextSiblingElement(QStringLiteral("piece")))
    {
        const QString pieceSize = element.attribute(QStringLiteral("size"));
        if (!pieceSize.isEmpty())
        {
            referenceLabel = pieceSize;
            break;
        }
    }

    for (const QString &sizeId : sizeIds)
    {
        const QString label = labelById.value(sizeId, sizeId);

        CutSize size;
        size.label = label;
        size.reference = !referenceLabel.isEmpty()
                         && (referenceLabel == label
                             || label.endsWith(QLatin1String(" ") + referenceLabel));

        // A piece the payload NAMES but does not DRAW is a file written before the outline was
        // carried. Counted, because « no cutting quantity » would be the wrong thing to say about a
        // pattern whose quantities are all there.
        int withoutOutline = 0;
        // The two figures version 3 added, counted for the same reason: a payload that names a piece
        // without them is a payload from another vintage, and no amount of laying out can repair it.
        int withoutAlignment = 0;
        int withoutInk = 0;

        for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece"));
             !element.isNull(); element = element.nextSiblingElement(QStringLiteral("piece")))
        {
            // The layer a piece belongs to, as the writer named it. Two forms are accepted: the
            // layer's written name, which is what the drawing shows and what a reader navigates by,
            // and the bare axis value an earlier writer put there. The second is only a suffix of
            // the first — « Taille 46 » ends with « 46 » — and accepting it costs one comparison
            // while refusing it makes every file of that vintage unreadable.
            const QString pieceSize = element.attribute(QStringLiteral("size"));
            if (pieceSize != label
                && !label.endsWith(QLatin1String(" ") + pieceSize))
            {
                continue;
            }

            const QString name = element.attribute(QStringLiteral("name"));

            // ⚠️ **The name a piece has, and the name the drawing gives it are not the same string.**
            // Ostara keys the drawn group on an id — spaces become underscores — and names the piece
            // with its real name: « Ourlet dos » is drawn as `Ourlet_dos`. Rabnest lists the pieces
            // from the drawing, so a comparison on the name alone drops every piece whose name has a
            // space in it, silently, and a pattern of nine pieces is planned with six.
            const QString slug = Slug(name);
            if (!kept.contains(name) && !kept.contains(slug))
            {
                continue;
            }

            const bool onFold =
                element.attribute(QStringLiteral("onFold")) == QLatin1String("yes");
            const bool storedFolded =
                element.attribute(QStringLiteral("stored")) == QLatin1String("folded");
            const QString stored = element.attribute(QStringLiteral("outline"));
            const QString alternative = element.attribute(QStringLiteral("outlineAlt"));

            // ── L'écart d'alignement ───────────────────────────────────────────────────────────
            //
            // The sizes of one piece do not superimpose in the pattern — one grows from its centre,
            // the other is pinned at its fold — so Ostara computes an offset per size and writes it
            // here, in pixels, in the frame of the contour. The contours below are written SHIFTED
            // BY NOTHING; the offset is applied to every coordinate the piece carries, once, right
            // here. Both consumers want the same answer: the engine unions the sizes into one
            // footprint, and that union means nothing unless they lie on top of each other.
            //
            // ⚠️ **All of them, and not the contour alone.** The fold axis and the grain line are
            // in the same frame, and a piece placed by its contour but drawn with an unshifted
            // grain line would carry its line beside it.
            const QVector<double> shift = Numbers(element.attribute(QStringLiteral("alignDelta")));
            if (shift.size() < 2)
            {
                ++withoutAlignment;
                continue;
            }
            const QPointF alignDelta(shift.at(0), shift.at(1));

            // Which of the two forms is laid is the OUTPUT's decision, taken here once. A print lays
            // the form the payload carries, whichever it is — it is reproducing the document, not
            // re-forming it — and only a cutting plan asks the cloth, which is the one thing that
            // can make a whole piece go on folded fabric. A piece with no fold has one form only,
            // and `alternative` is then empty.
            const bool storedIsTheOneLaid = cloth == Cloth::AsDrawn || !onFold
                                            || (cloth == Cloth::Folded) == storedFolded;
            QVector<QPointF> contour = Outline(storedIsTheOneLaid ? stored : alternative);

            if (contour.size() < 3)
            {
                ++withoutOutline;
                continue;
            }

            contour = Shifted(contour, alignDelta);

            // The contour as the canvas draws it — notches included, and the sewing line when the
            // piece shows one. Required for the form the payload CARRIES: a piece drawn without its
            // notches looks finished and cannot be mounted, and a reader that silently fell back on
            // the plain outline would be hiding that. The attribute may be EMPTY, and that is not
            // the same thing: a piece whose main path is hidden and that has no seam allowance draws
            // nothing, and the payload says so by carrying nothing.
            //
            // ⚠️ **One ink per form, and the form LAID is the one DRAWN.** A folded piece is stored
            // as its half and cut whole on flat cloth: the payload carries both inks, the same choice
            // that picked the contour picks its ink.
            QVector<QPolygonF> ink;
            const QString inkAttribute = storedIsTheOneLaid ? QStringLiteral("outlineInk")
                                                            : QStringLiteral("outlineInkAlt");
            if (!element.hasAttribute(inkAttribute))
            {
                if (storedIsTheOneLaid)
                {
                    ++withoutInk;
                    continue;
                }

                // La forme RÉFORMÉE d'un payload écrit avant que son encre ne soit portée : le
                // contour est là, les crans non. On dessine le contour — une pièce sans ses crans
                // reste une pièce juste — plutôt que de refuser le plan entier pour un détail que
                // la prochaine génération apportera.
                ink = {QPolygonF(contour)};
            }
            else
            {
                ink = Shifted(InkPaths(element.attribute(inkAttribute)), alignDelta);
            }

            const int cutFabric = element.attribute(QStringLiteral("cutFabric")).toInt();
            if (cutFabric <= 0)
            {
                // Not cut in this cloth at all — a piece somebody left blank in the cartouche, which
                // is not « once » but « not at all », and takes no part in the plan.
                continue;
            }

            const QVector<double> axis = Numbers(element.attribute(QStringLiteral("foldAxis")));
            const QVector<double> grainLine =
                Numbers(element.attribute(QStringLiteral("grainLine")));
            // Its own children, or its group's — the payload states which by carrying them (or not).
            // Filed under THIS size, because the payload seats the drawing on each size separately.
            {
                PieceDecor one;
                const QDomElement own = element.firstChildElement(QStringLiteral("decor"));
                if (!ReadDecor(own, &one))
                {
                    one = groupDecor.value(element.attribute(QStringLiteral("group")));
                }
                if (!one.content.isEmpty())
                {
                    // The decor is written in the contour's frame, so it takes the alignment too —
                    // by a TRANSLATION wrapped around it, never by re-writing its numbers: its ink
                    // comes from Ostara's own serializer, and a second reader that moved every
                    // coordinate would be a second serializer allowed to disagree with the first.
                    // The box moves with it, and the two moves do not add up twice: the box is the
                    // SVG's viewBox AND the offset the item is placed at, so shifting one alone
                    // would cancel itself out.
                    if (!alignDelta.isNull())
                    {
                        one.content = QStringLiteral("<g transform=\"translate(%1,%2)\">")
                                          .arg(QString::number(alignDelta.x(), 'f', 3),
                                               QString::number(alignDelta.y(), 'f', 3))
                                      + one.content + QStringLiteral("</g>");
                        one.box.translate(alignDelta);
                    }

                    // Both spellings of the name, for the reason the piece lookup below accepts both:
                    // the drawing writes `Ourlet_dos`, the payload names « Ourlet dos », and a plan
                    // looks its decor up by the name it plans with.
                    size.decor.insert(name, one);
                    size.decor.insert(Slug(name), one);
                }
            }

            CutPiece pose;
            pose.name = name;
            pose.outline = contour;
            pose.ink = ink;
            pose.onFold = onFold;
            // The payload's own key, which is the group's for a member and the piece's name
            // otherwise. Fallen back on the name and the group id, so a payload written before the
            // key was carried still names something a placement can be found under.
            pose.groupKey = element.attribute(QStringLiteral("group"));
            pose.footprint = element.attribute(QStringLiteral("key"));
            if (pose.footprint.isEmpty())
            {
                pose.footprint = pose.groupKey.isEmpty()
                                     ? name
                                     : QStringLiteral("g%1").arg(pose.groupKey);
            }
            pose.grainKnown = element.hasAttribute(QStringLiteral("grain"));
            pose.grain = element.attribute(QStringLiteral("grain")).toDouble();
            pose.arrows = element.attribute(QStringLiteral("arrows")).toInt();
            pose.cutFabric = cutFabric;
            pose.cutLining = element.attribute(QStringLiteral("cutLining")).toInt();
            // Both in the contour's frame, so both take the alignment with it.
            if (axis.size() >= 4)
            {
                pose.foldA = QPointF(axis.at(0), axis.at(1)) + alignDelta;
                pose.foldB = QPointF(axis.at(2), axis.at(3)) + alignDelta;
            }
            if (grainLine.size() >= 4)
            {
                pose.hasGrainLine = true;
                pose.grainA = QPointF(grainLine.at(0), grainLine.at(1)) + alignDelta;
                pose.grainB = QPointF(grainLine.at(2), grainLine.at(3)) + alignDelta;
            }

            // ── La quantité ─────────────────────────────────────────────────────────────────────
            //
            // One pose cuts two thicknesses on folded cloth, and the fold makes the second piece: a
            // piece cut four times is laid twice. Rounded UP — one stays one, three becomes two,
            // five becomes three. A piece cut ON the fold is not divided: one pose on the fold gives
            // ONE whole piece, the fold completing it rather than doubling it.
            //
            // ⚠️ **A print lays every piece ONCE, and the quantity is none of its business.** The
            // payload writes one record per piece per size — the drawing shows that piece once —
            // and « cut 4 » is a figure the cartouche carries, not a request to print it four
            // times. Only a cutting plan expands it, because only a plan produces pieces to cut.
            int quantity = cloth == Cloth::AsDrawn ? 1 : cutFabric;
            if (cloth == Cloth::Folded && !onFold)
            {
                quantity = (cutFabric + 1) / 2;
            }
            if (quantity < 1)
            {
                quantity = 1;
            }

            for (int copy = 0; copy < quantity; ++copy)
            {
                CutPiece one = pose;
                // ⚠️ Its own key per pose: the footprint is the union of everything sharing a key,
                // so two copies under one key would be one footprint and the second would exist
                // nowhere — silently.
                one.key = copy > 0 ? name + QStringLiteral("-c%1").arg(copy + 1) : name;
                size.poses.append(one);

                // ── La jumelle ────────────────────────────────────────────────────────────────
                //
                // On folded cloth a piece cut on the fold is laid as its half, and that half is
                // drawn for one edge of the fold. The mirror opens the other: one of the two is
                // laid, the other exists so that either edge can receive it. Both are emitted here
                // and the plan keeps the one whose edge matches the fold it is given.
                if (cloth == Cloth::Folded && onFold)
                {
                    const double axisX = CentreX(one.outline);

                    CutPiece twin = one;
                    twin.key = one.key + QStringLiteral("-m");
                    twin.outline = Mirrored(one.outline, axisX);
                    twin.ink = Mirrored(one.ink, axisX);
                    twin.foldA = QPointF(2.0 * axisX - one.foldA.x(), one.foldA.y());
                    twin.foldB = QPointF(2.0 * axisX - one.foldB.x(), one.foldB.y());
                    twin.grain = MirroredGrain(one.grain);
                    if (one.hasGrainLine)
                    {
                        // La ligne suit le miroir, comme le contour : sans ça elle resterait du
                        // côté d'où la pièce vient de partir.
                        twin.grainA = QPointF(2.0 * axisX - one.grainA.x(), one.grainA.y());
                        twin.grainB = QPointF(2.0 * axisX - one.grainB.x(), one.grainB.y());
                    }
                    size.poses.append(twin);
                }
            }
        }

        // ⚠️ **A piece the payload names but does not draw is a hard stop, never a smaller plan.**
        // The pieces that have an outline would lay out perfectly, and the result would be a plan
        // that looks finished and is missing seven of its nine pieces — the kind of wrong that is
        // only found once the cloth is cut. The only honest answer is to refuse the size.
        if (withoutOutline > 0)
        {
            if (error != nullptr)
            {
                *error = QObject::tr("This PDF was exported by an older Ostara: it does not carry "
                                     "the outline of its pieces. Export it again.");
            }
            return false;
        }

        // Same rule, for the two figures version 3 added. Laying the pieces without them would give
        // a plan drawn without its notches, over sizes that never line up — a plan that looks
        // finished from the first sheet to the last.
        if (withoutAlignment > 0 || withoutInk > 0)
        {
            if (error != nullptr)
            {
                *error = QObject::tr("This PDF was exported by an older Ostara: it does not carry "
                                     "the drawn contour of its pieces, nor the offset that lines "
                                     "their sizes up. Export it again.");
            }
            return false;
        }

        if (size.poses.isEmpty())
        {
            if (error != nullptr)
            {
                *error = QObject::tr("No piece of \"%1\" can be cut: none of them carries a "
                                     "cutting quantity for the fabric.")
                             .arg(label);
            }
            return false;
        }

        out.append(size);
    }

    if (out.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QObject::tr("No size of this pattern carries a piece Rabnest can lay out.");
        }
        return false;
    }

    return true;
}
