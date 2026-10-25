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
/// The piece AS DRAWN, one subpath per polygon — `M x,y` … `L x,y` … repeated.
///
/// Ostara writes it with the same serializer as the drawing's own `<path d>` (`M`/`L`, three
/// decimals, no `Z`), and splits the subpaths with `M`. Nothing here interprets a curve: the sewing
/// line, the seam allowance and the notches are polygons all the way down, and a `C` would be read as
/// more of the same polyline, which is what a cutting plan's own outline reader has always done.
///
/// ⚠️ **`outlineInk` is not only contours, and a two-point subpath is not a mistake.** Ostara fills
/// it from `InkOf()`, which takes every child of the piece's own item that is NOT an information
/// block (`OSvg::IsInfoChild`) — so a PLACE LABEL travels here, and the place label is a shape, not
/// always a polygon: `Cross`, `Segment`, `T-shaped` and `H-shaped` are drawn with STROKES, and a
/// stroke is two points. So is the mirror line of a piece shown whole.
///
/// Dropping everything under three points dropped exactly those: Devant's cross (two 1 cm segments,
/// the place label of the app's own sample pattern) never reached the plan while Dos's triangle (a
/// real polygon) did — one piece labelled, the next bare, and nothing in the file to explain it. A
/// two-point subpath is kept and stroked like any other trace; it has no area, and the plan fills
/// what it draws, so a segment paints as the line it is.
auto InkPaths(const QString &text) -> QVector<QPolygonF>
{
    QVector<QPolygonF> paths;

    for (const QString &chunk : text.split(QLatin1Char('M'), Qt::SkipEmptyParts))
    {
        const QVector<double> values = Numbers(chunk);
        // Two points is a stroke (a place label's arm, a mirror line); one is not a drawing at all.
        if (values.size() < 4)
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
/// The inverse of @p matrix, or false when it collapses the plane.
///
/// Needed to re-lay a child whose own `transform` is left where the writer put it: the reflection that
/// moves it is built in the child's LOCAL frame, and expressing it in the decor's frame takes a
/// `M · F · M⁻¹`.
auto Inverted(const Matrix &matrix, Matrix *out) -> bool
{
    const double det = matrix.a * matrix.d - matrix.b * matrix.c;
    if (std::abs(det) < 1e-12)
    {
        return false;
    }

    Matrix inverse;
    inverse.a = matrix.d / det;
    inverse.b = -matrix.b / det;
    inverse.c = -matrix.c / det;
    inverse.d = matrix.a / det;
    inverse.e = -(inverse.a * matrix.e + inverse.c * matrix.f);
    inverse.f = -(inverse.b * matrix.e + inverse.d * matrix.f);

    *out = inverse;
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
auto MatrixAttr(const Matrix &matrix) -> QString
{
    return QStringLiteral("matrix(%1,%2,%3,%4,%5,%6)")
        .arg(QString::number(matrix.a, 'f', 6), QString::number(matrix.b, 'f', 6),
             QString::number(matrix.c, 'f', 6), QString::number(matrix.d, 'f', 6),
             QString::number(matrix.e, 'f', 3), QString::number(matrix.f, 'f', 3));
}

auto Shifted(const QPointF &delta) -> Matrix
{
    Matrix out;
    out.e = delta.x();
    out.f = delta.y();
    return out;
}

auto FlipX() -> Matrix
{
    Matrix out;
    out.a = -1.0;
    return out;
}

/// The reflection about the vertical line `x = axisX`, as a matrix.
auto Reflected(double axisX) -> Matrix
{
    return Compose(Shifted(QPointF(2.0 * axisX, 0.0)), FlipX());
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

/// @param skipOwnTransform when true, the element's OWN `transform` is ignored and only its
///        descendants' are applied — the box of what the element draws in its parent's frame is not
///        what this returns, it is the box in the element's own frame. That is the frame a reflection
///        is written in.
auto DecorBox(const QDomElement &element, const Matrix &outer, GrowingBox *box,
              bool skipOwnTransform = false) -> void
{
    const Matrix matrix = skipOwnTransform
                              ? outer
                              : Compose(outer,
                                        ParseTransform(element.attribute(QStringLiteral("transform"))));

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
/// La miniature, telle que le payload l'écrit : un chemin TRACÉ et non rempli, sans marque.
///
/// **Ostara ne marque que le droit-fil et le pli** (`ostara:grain`, `ostara:fold`). La miniature est
/// écrite par le bloc d'informations comme un seul chemin absolu, `fill="none" stroke="black"`, et ses
/// mots dans un groupe à part — il n'y a donc aucun attribut sur lequel la reconnaître, seulement sa
/// forme. Vérifié sur `32-46-160-A0Roll.pdf` : quatre pièces en portent une (« Devant x1 pli », « Dos
/// x1 pli », « Manche poignet », « Parementure devant x1 pli »), et c'est chaque fois le SEUL élément
/// du bloc dans ce cas — tous les autres sont remplis (texte, cartouche, logo) ou marqués.
///
/// ⚠️ **Point ouvert : un marqueur vaudrait mieux que cette forme.** Ostara sait qu'elle écrit une
/// miniature (`WriteMiniature`) ; le jour où elle le dira dans le fichier, cette fonction disparaîtra.
auto IsMiniature(const QDomElement &element) -> bool
{
    if (element.tagName() != QLatin1String("path"))
    {
        return false;
    }
    if (element.hasAttribute(QStringLiteral("ostara:grain"))
        || element.hasAttribute(QStringLiteral("ostara:fold")))
    {
        return false;
    }

    const QString fill = element.attribute(QStringLiteral("fill"));
    const QString stroke = element.attribute(QStringLiteral("stroke"));
    return (fill.isEmpty() || fill == QLatin1String("none")) && !stroke.isEmpty()
           && stroke != QLatin1String("none");
}

auto ReadDecor(const QDomElement &decorElement, PieceDecor *out, bool withMiniature = true) -> bool
{
    if (out == nullptr || decorElement.isNull())
    {
        return false;
    }

    QString content;
    QXmlStreamWriter writer(&content);
    GrowingBox box;

    QVector<QDomElement> children;
    for (QDomElement child = decorElement.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        children.append(child);
    }

    for (int index = 0; index < children.size(); ++index)
    {
        const QDomElement child = children.at(index);

        // ⚠️ **Une miniature ne sert à rien sur un plan de coupe, et elle y sort de sa pièce.**
        // Décision de Sophie (25-10-2026) : son placement n'est pas prévu pour le miroir, donc une
        // pièce retournée la fait déborder. Le print la garde — c'est le document.
        if (!withMiniature && IsMiniature(child))
        {
            // ⚠️ **Et ses MOTS partent avec son dessin.** Le bloc d'informations écrit le dessin de
            // la miniature, puis ses valeurs dans un groupe à part, juste après lui — laisser le
            // groupe ferait flotter les chiffres d'une miniature qui n'est plus là (constaté sur le
            // plan de coupe : le dessin parti, les textes restés). Les miniatures ferment les
            // enregistrements (`WriteMiniature` est appelé en dernier), donc le groupe qui suit un
            // dessin de miniature est le sien et rien d'autre.
            if (index + 1 < children.size()
                && children.at(index + 1).tagName() == QLatin1String("g"))
            {
                ++index;
            }
            continue;
        }

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

/// Whether @p element holds a grain line or a fold mark anywhere below it — what tells the projection
/// wrapper apart from a record group, which never does.
auto HasTrace(const QDomElement &element) -> bool
{
    for (QDomElement child = element.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        if (child.tagName() == QLatin1String("path")
            && (child.hasAttribute(QStringLiteral("ostara:grain"))
                || child.hasAttribute(QStringLiteral("ostara:fold"))))
        {
            return true;
        }
        if (HasTrace(child))
        {
            return true;
        }
    }
    return false;
}

/// The `<decor>` children that are DRAWINGS, the projection wrapper unwrapped.
///
/// ⚠️ **The reference size and the others are not shaped the same.** Ostara seats the decor on each
/// size by a pure translation, and writes that translation as a `<g>` around the WHOLE body — so the
/// first size carries its children directly and every other size carries one `<g>` holding them all.
/// Re-laying that wrapper as one child would move the block as a rigid whole and never negate the
/// angle of a cartouche, while the reference size negates it: one piece, two drawings, depending on
/// the size asked for.
///
/// ⚠️ **One level only, and that is the whole point.** A fold mark and its caption are each wrapped in
/// a translation of their OWN (`foldProjection`), and those wrappers look exactly like the projection
/// one — a `<g>` holding a marked trace. Unwrapping one of them folded ITS translation into the
/// translation every child shares, which moved every other child by it: the fold's caption came off
/// the mark, on every size but the reference one. Only the outermost wrapper — the one and only child
/// of `<decor>` — is the projection, and it is the only thing unwrapped here.
///
/// @param outer carries the wrapper's translation, folded in on the right: the payload's own
///        coordinates are the body's, so its translation applies BEFORE the shift and the mirror.
auto DecorLeaves(const QDomElement &decorElement, Matrix *outer) -> QVector<QDomElement>
{
    const auto isPureTranslation = [](const Matrix &m) {
        return std::abs(m.a - 1.0) < 1e-9 && std::abs(m.d - 1.0) < 1e-9
               && std::abs(m.b) < 1e-9 && std::abs(m.c) < 1e-9;
    };

    QDomElement only;
    int count = 0;
    for (QDomElement child = decorElement.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        only = child;
        ++count;
    }

    QDomElement body = decorElement;
    if (count == 1 && only.tagName() == QLatin1String("g") && HasTrace(only))
    {
        const Matrix own = ParseTransform(only.attribute(QStringLiteral("transform")));
        if (isPureTranslation(own))
        {
            *outer = Compose(*outer, own);
            body = only;
        }
    }

    QVector<QDomElement> leaves;
    for (QDomElement child = body.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        leaves.append(child);
    }
    return leaves;
}

//---------------------------------------------------------------------------------------------------------------------
/// The same decor, **re-laid on the other side of the piece — its children moved, never reflected.**
///
/// A piece cut on the fold is planned twice: as it is drawn, and mirrored. Its outline, its ink, its
/// fold line and its grain line all follow — the children it carries did not, and a cartouche, a
/// miniature and a set of annotations stayed where the unmirrored piece would put them.
///
/// ⚠️ **A plain reflection would move them AND mirror their glyphs, which is unreadable.** So each
/// child is re-laid instead: it keeps its own drawing, and only its placement is reflected. The
/// reflection of a placement is written per child, in the child's OWN frame, as
///
///     X = B · M · F · M⁻¹        with  B  the shift and the mirror,   M  the child's own transform,
///                                      F  the reflection about the child's own box centre.
///
/// whose linear part is `F · R(θ) · F = R(−θ)`: the box lands mirrored, the angle is NEGATED, and the
/// determinant stays +1 — the glyphs are never flipped. This is Ostara's own rule
/// (`MirrorDecor`, `oexportcanvas.cpp`), and it degenerates to a plain translation for a child drawn
/// without rotation.
///
/// ⚠️ **Two children are the exception, and both are told apart by their own marker.** The grain line
/// and the fold mark carry `ostara:grain` / `ostara:fold`: they are GEOMETRY, they hold no glyph, and
/// they are therefore REFLECTED — a translation would leave their arrows pointing the way they pointed
/// on the piece's other side, which is a fold mark that sits right and says the opposite. Everything
/// else is re-laid.
///
/// ⚠️ **One child keeps its angle instead of negating it, and it cannot be helped.** The fold's
/// caption is written as a path whose glyphs are already turned in the coordinates — its rotation is
/// not recoverable from the file, only the box it occupies. It therefore MOVES with the mirror and
/// keeps reading the way it read, which is what the requirement asks for and is the most a reader can
/// honour.
///
/// @param decorElement the `<decor>` of the payload
/// @param alignDelta   the offset the sizes of one piece are aligned by — applied BEFORE the mirror,
///                     as it is to the contour
/// @param axisX        the vertical line the twin was reflected about
auto ReadMirroredDecor(const QDomElement &decorElement, const QPointF &alignDelta, double axisX,
                       PieceDecor *out) -> bool
{
    if (out == nullptr || decorElement.isNull())
    {
        return false;
    }

    const Matrix place = Compose(Reflected(axisX), Shifted(alignDelta));


    // The projection wrapper is folded into `place` here, so every leaf below is re-laid in the one
    // frame the twin's outline lives in.
    Matrix outer = place;
    const QVector<QDomElement> leaves = DecorLeaves(decorElement, &outer);

    QString content;
    QXmlStreamWriter writer(&content);
    GrowingBox box;

    // ⚠️ A trace is a path with the marker — and it is TWO shapes, not one. Most arrive wrapped in a
    // translation of their own, so the subtree has to be asked; but the plain one is the element
    // ITSELF, with no child at all, and asking only the descendants missed it. Missed, it took the
    // re-lay path instead of the reflection: the mark was moved to the other side of the piece and its
    // arrows stayed pointing the way they pointed before — a fold mark sitting right and saying the
    // opposite. The element is asked first, then what hangs under it.
    const auto isTrace = [](const QDomElement &element)
    {
        if (element.tagName() == QLatin1String("path")
            && (element.hasAttribute(QStringLiteral("ostara:grain"))
                || element.hasAttribute(QStringLiteral("ostara:fold"))))
        {
            return true;
        }
        return HasTrace(element);
    };

    for (const QDomElement &child : leaves)
    {
        const Matrix own = ParseTransform(child.attribute(QStringLiteral("transform")));

        Matrix inverse;
        if (!Inverted(own, &inverse))
        {
            return false;
        }

        GrowingBox local;
        DecorBox(child, Matrix{}, &local, /*skipOwnTransform=*/true);
        const QRectF localRect = local.Rect();
        const double centre = localRect.isNull() ? 0.0 : localRect.center().x();

        // F: reflect about the child's own box centre, in its own frame.
        const Matrix flip = Compose(Shifted(QPointF(2.0 * centre, 0.0)), FlipX());

        // ⚠️ **A grain line and a fold mark are REFLECTED, not re-laid.** They carry no glyph, and a
        // translation would leave their ARROWS pointing the way they pointed on the other side — the
        // mark would sit in the right place and say the wrong thing. Ostara never has the problem
        // because it draws them from the piece it has already mirrored.
        Matrix step = outer;
        if (!isTrace(child))
        {
            step = Compose(step, own);
            step = Compose(step, flip);
            step = Compose(step, inverse);
        }

        writer.writeStartElement(QStringLiteral("g"));
        writer.writeAttribute(QStringLiteral("transform"), MatrixAttr(step));
        WriteNode(writer, child);
        writer.writeEndElement();
    }

    for (QDomElement child = decorElement.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        DecorBox(child, Matrix{}, &box);
    }

    if (content.isEmpty() || box.Rect().isNull())
    {
        return false;
    }

    const QRectF raw = box.Rect();
    const QRectF placed = raw.translated(alignDelta);
    out->content = content;
    // The box is the SVG's viewBox, so it travels with the content: mirrored about the same axis,
    // same width and height.
    out->box = QRectF(2.0 * axisX - placed.right(), placed.top(), placed.width(), placed.height());
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
auto ReferenceSize(const ImportedDocument &document) -> QString
{
    QDomDocument payload;
    if (!payload.setContent(document.payload()))
    {
        return {};
    }

    const QDomElement metadata =
        payload.documentElement().firstChildElement(QStringLiteral("metadata"));
    const QDomElement pieces = metadata.firstChildElement(QStringLiteral("pieces"));
    if (pieces.isNull())
    {
        return {};
    }

    // The attribute when the payload carries one (version 5 and after); otherwise the size of the
    // FIRST `<piece>`, which is what Ostara wrote before it named it — the smallest of the range,
    // and the only one whose drawing carries the children.
    QString label = pieces.attribute(QStringLiteral("reference"));
    if (label.isEmpty())
    {
        for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece"));
             !element.isNull(); element = element.nextSiblingElement(QStringLiteral("piece")))
        {
            label = element.attribute(QStringLiteral("size"));
            if (!label.isEmpty())
            {
                break;
            }
        }
    }
    if (label.isEmpty())
    {
        return {};
    }

    // The layer's own id, by the rule `Pattern::Load` uses for the same label: equal, or the label
    // carried at the end of a longer one.
    for (const ImportedLayer &layer : document.layers())
    {
        if (layer.decor)
        {
            continue;
        }
        if (layer.label == label || layer.label.endsWith(QLatin1String(" ") + label))
        {
            return layer.id;
        }
    }

    return {};
}

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
auto Pattern::Load(const ImportedDocument &document, const QStringList &sizeIds,
                   const QStringList &pieceNames, Cloth cloth, QVector<CutSize> &out,
                   QString *error, Material material) -> bool
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

    // **Version 5 : le décor appartient à UNE taille.** Chaque `<piece>` porte les enfants de SA
    // taille, tels que Graded les a construits pour elle — c'est ce qui fait que le décor ne dérive
    // plus d'une taille à l'autre. La 4 donnait à chaque membre d'un groupe ses propres enfants, mais
    // recopiait le décor d'une seule taille dans toutes les autres ; la 3 les lui refusait.
    //
    // La 4 et la 3 restent lisibles : leur bloc est celui de la référence, et c'est l'ancrage au pli
    // qui les rattrape tant bien que mal — voir plus bas, où `decorPerSize` décide.
    //
    // La 2 n'a ni le contour dessiné ni l'offset d'alignement : un lecteur ne pourrait superposer ses
    // tailles qu'en devinant un décalage qu'il n'a jamais reçu, et dessinerait des pièces sans crans.
    // La 1 n'a aucun décor. Lire la version la plus proche serait pire que refuser : le plan poserait
    // dix pièces et pas une ligne de droit-fil, sans le dire. **Un lecteur qui ne trouve pas ce qu'on
    // lui demande le dit.**
    const QString payloadVersion = pieces.attribute(QStringLiteral("version"));
    if (payloadVersion != QLatin1String("3") && payloadVersion != QLatin1String("4")
        && payloadVersion != QLatin1String("5") && payloadVersion != QLatin1String("6"))
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF was exported by a version of Ostara this Rabnest does not "
                                 "know (payload version %1): it carries neither the drawn contour of "
                                 "its pieces nor the offset that lines their sizes up. Export it "
                                 "again.")
                         .arg(payloadVersion.isEmpty() ? QStringLiteral("none") : payloadVersion);
        }
        return false;
    }

    // Le décor est-il écrit PAR TAILLE (5) ou recopié de la référence dans chaque `<piece>` (3 et 4) ?
    // C'est toute la différence : au-dessus de 4, un bloc est déjà à sa place et le décaler une
    // seconde fois le déplace pour de bon ; en dessous, il faut l'ancrer.
    const bool decorPerSize = payloadVersion == QLatin1String("5");

    /// **Version 6 : le décor est le DESSIN de la référence, écrit une fois, et il ne bouge pas.**
    ///
    /// Le canvas graded ne dessine les enfants que sur sa couche de référence, et les tailles sont
    /// superposées : un cartouche occupe donc UNE place, la même pour toutes les tailles. Mesuré sur
    /// le SVG graded, le bloc de « Manche » reste à x = 7068 du début à la fin de la plage, tandis que
    /// la pièce, elle, s'en va de 221 px. Le décor prend donc l'écart d'alignement de PERSONNE : la
    /// pièce le prend, lui non, et c'est ce qui le laisse à sa place.
    ///
    /// ⚠️ Le lui donner, comme le faisaient les versions 3 à 5, le fait glisser avec la pièce : sa
    /// fraction restait constante là où le canvas la fait varier (0.853 à 0.752 sur la même pièce),
    /// et l'écart atteignait 5 cm à la taille 46.
    const bool decorShared = payloadVersion == QLatin1String("6");

    QHash<QString, QString> labelById;
    for (const ImportedLayer &layer : document.layers())
    {
        labelById.insert(layer.id, layer.label);
    }

    const QSet<QString> kept(pieceNames.cbegin(), pieceNames.cend());

    // ── Le décor ────────────────────────────────────────────────────────────────────────────────
    //
    // Read from the PAYLOAD, and never from the drawing again. **Depuis la version 5, chaque
    // `<piece>` porte le décor de SA taille** — Graded construit les enfants de chaque pièce à la
    // taille de sa couche, et le payload le dit tel quel : un plan de coupe de la taille 46 trouve
    // les enfants de 46 là où le dessin les a posés, dans le repère de son `outline`, et n'a qu'à
    // les poser avec la pièce. Rien n'est mesuré, décalé ni ancré : le payload énonce une position.
    //
    // Avant la 5, le même bloc était recopié de la référence dans toutes les `<piece>`, et c'est
    // l'ancrage (plus bas) qui tentait de le rattraper — d'où le drapeau `decorPerSize`.
    //
    // A grouped member carries none of its own — Graded and Details both hand its decor to the group
    // — so the group's block is read first and stands in for every member that lacks one.
    QHash<QString, PieceDecor> groupDecor;
    // The element the block came from, kept for the same reason the block is: a twin of a grouped
    // member is re-laid from the DRAWING, and only the element can be re-laid — a reading is one answer.
    QHash<QString, QDomElement> groupDecorSource;

    // One plan per size, as Ostara does it: a cutting plan is laid for one size, and asking for
    // three gives three plans, not one plan over three sizes.
    //
    // The reference size — **the one the payload NAMES** since version 5 (`<pieces reference="…">`),
    // and the first `<piece>` written before that, which is the smallest of the range. It is the size
    // whose drawing carries the one cartouche of a print; read here, before anything is filtered out,
    // because a piece the run does not keep still names its layer.
    QString referenceLabel = pieces.attribute(QStringLiteral("reference"));
    if (referenceLabel.isEmpty())
    {
        for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece"));
             !element.isNull(); element = element.nextSiblingElement(QStringLiteral("piece")))
        {
            const QString pieceSize = element.attribute(QStringLiteral("size"));
            if (!pieceSize.isEmpty())
            {
                referenceLabel = pieceSize;
                break;
            }
        }
    }

    // ── L'écart d'alignement de la TAILLE DE RÉFÉRENCE, par pièce ───────────────────────────────
    //
    // ⚠️ **Le dessin du jumeau est celui de la référence, retourné une fois pour toutes** — comme le
    // bloc de la pièce, qui est le même octet pour toutes les tailles. L'écart qui le pose ne peut
    // donc pas être celui de la taille demandée : à la plus grande taille, où cet écart vaut zéro, le
    // contenu partait vers la droite de tout ce que la référence, elle, portait.
    QHash<QString, QPointF> referenceAlignDelta;
    for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece")); !element.isNull();
         element = element.nextSiblingElement(QStringLiteral("piece")))
    {
        if (element.attribute(QStringLiteral("size")) != referenceLabel)
        {
            continue;
        }
        const QVector<double> shift = Numbers(element.attribute(QStringLiteral("alignDelta")));
        if (shift.size() >= 2)
        {
            referenceAlignDelta.insert(element.attribute(QStringLiteral("name")),
                                       QPointF(shift.at(0), shift.at(1)));
        }
    }

    // ── Le repère d'un groupe ───────────────────────────────────────────────────────────────────
    //
    // ⚠️ **A group's children are not written in the pieces' frame, and placing them with a piece is
    // what sent them off the canvas.** Ostara writes them in a frame of the group's own, whose ORIGIN
    // is the top-left of the union of its members' boxes — said by the canvas itself, which brings the
    // members' boxes in by exactly that translation before building the children (`oexportcanvas.cpp`,
    // « the origin is the union's top-left corner »). A piece's block needs no such thing because the
    // payload projects it onto the piece; a group's is written once and projected nowhere.
    //
    // The union is taken at the REFERENCE size, and taken here because the group is written once: a
    // translation per size would claim the block had been re-seated per size, and it has not.
    QHash<QString, QPointF> groupOrigin;
    {
        QHash<QString, QRectF> unions;
        for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece"));
             !element.isNull(); element = element.nextSiblingElement(QStringLiteral("piece")))
        {
            const QString groupId = element.attribute(QStringLiteral("group"));
            if (groupId.isEmpty())
            {
                continue;
            }

            const QString pieceSize = element.attribute(QStringLiteral("size"));
            if (!referenceLabel.isEmpty() && pieceSize != referenceLabel)
            {
                continue;
            }

            const QVector<QPointF> outline = Outline(element.attribute(QStringLiteral("outline")));
            if (outline.size() < 3)
            {
                continue;
            }

            const QRectF box = QPolygonF(outline).boundingRect();
            unions[groupId] = unions.contains(groupId) ? unions.value(groupId).united(box) : box;
        }

        for (auto it = unions.cbegin(); it != unions.cend(); ++it)
        {
            groupOrigin.insert(it.key(), it.value().topLeft());
        }
    }

    // ── Le décor, lu dans le DESSIN que le payload transporte ────────────────────────────────────
    //
    // ⚠️ **Le payload EST un SVG graded.** L'export écrit le même document deux fois, par le même
    // écrivain : le fichier visible, et le payload qui part sous `/PieceInfo` (`oexportpdf.cpp`,
    // `WriteGradedSvg`). Ce document porte donc le décor **une seule fois**, dans son calque
    // « Mise en page » — c'est le dessin de l'écran, celui que Graded a placé pour toutes les tailles.
    //
    // Rabnest lisait jusqu'ici les `<decor>` que `OExportPieces` refabrique **par taille** : un
    // SECOND dessin, recalculé, et c'est lui qui décalait tout sauf à la taille de référence — la
    // seule où les deux se confondent. Le calque de mise en page n'a pas de second exemplaire.
    //
    // Il est écrit dans le repère de la scène d'export ; les pièces du payload sont dans celui du
    // patron. L'écart est une translation par pièce, mesurée UNE fois, à la taille de référence :
    // c'est là que les deux repères se superposent, à l'écart d'alignement près.
    QHash<QString, QDomElement> drawnDecor;  // slug → le wrapper du calque de mise en page
    QHash<QString, QPointF> drawnShift;      // slug → scène moins patron, à la référence
    {
        const QDomElement root = payload.documentElement();

        // ⚠️ **DÉSARMÉ : le pont par `ostara:id` est écrit mais il place encore mal le décor.**
        // Mesuré sur le payload du 25-10 à 14 h : « Manche » sort à 5205 au lieu de 4196 (1009 px
        // de trop) et « Dos » à 1855 au lieu de 439. Tant que ce n'est pas juste, le `<decor>` de
        // `<pieces>` reste le seul chemin sûr pour ce que Sophie voit.
        if (root.attribute(QStringLiteral("ostara:drawing")) == QLatin1String("graded"))
        {
            const QString referenceLayerId =
            QString(referenceLabel).replace(QLatin1Char(' '), QLatin1Char('_'));

        QDomElement layoutLayer;
        QDomElement referenceLayer;
        for (QDomElement layer = root.firstChildElement(QStringLiteral("g")); !layer.isNull();
             layer = layer.nextSiblingElement(QStringLiteral("g")))
        {
            if (layer.attribute(QStringLiteral("inkscape:groupmode")) != QLatin1String("layer"))
            {
                continue;
            }
            if (layer.attribute(QStringLiteral("ostara:layout")) == QLatin1String("yes"))
            {
                layoutLayer = layer;
            }
            else if (!referenceLayerId.isEmpty()
                     && layer.attribute(QStringLiteral("id")) == referenceLayerId)
            {
                referenceLayer = layer;
            }
        }

        // One wrapper per owner, keyed by the id the drawing gives it — the sanitized name, which is
        // exactly what `Slug` reproduces.
        for (QDomElement wrapper = layoutLayer.firstChildElement(QStringLiteral("g")); !wrapper.isNull();
             wrapper = wrapper.nextSiblingElement(QStringLiteral("g")))
        {
            const QString id = wrapper.attribute(QStringLiteral("id"));
            if (!id.isEmpty())
            {
                drawnDecor.insert(id, wrapper);
            }
            // La clé qui ne se trompe pas : l'identifiant de la pièce, porté par le wrapper.
            const QString owner = wrapper.attribute(QStringLiteral("ostara:id"));
            if (!owner.isEmpty())
            {
                drawnDecor.insert(QStringLiteral("#") + owner, wrapper);
            }
        }

        // The reference layer's ink, so each piece's two frames can be told apart.
        //
        // ⚠️ **La pièce est l'objet de PREMIER NIVEAU du calque**, jamais la copie dessinée dans
        // son groupe. Le même nom apparaît deux fois dans une couche de taille : une fois dans le
        // groupe (`Manche`, à la position où le groupe la tient) et une fois comme vue du membre
        // (`Manche_1`, à sa propre place). C'est la SECONDE que le décor accompagne — mesuré sur
        // le SVG graded : décor 7101 − membre 6663 = 438, l'écart juste ; contre la copie du groupe
        // (5218) il valait 1884, ce qui n'a aucun sens.
        QHash<QString, QRectF> drawnInk;
        for (QDomElement child = referenceLayer.firstChildElement(QStringLiteral("g")); !child.isNull();
             child = child.nextSiblingElement(QStringLiteral("g")))
        {
            const QString id = child.attribute(QStringLiteral("id"));
            if (id.isEmpty())
            {
                continue;
            }
            GrowingBox box;
            DecorBox(child, Matrix{}, &box);
            if (!box.Rect().isNull())
            {
                drawnInk.insert(id, box.Rect());
                const QString owner = child.attribute(QStringLiteral("ostara:id"));
                if (!owner.isEmpty())
                {
                    drawnInk.insert(QStringLiteral("#") + owner, box.Rect());
                }
            }
        }

        if (!drawnInk.isEmpty())
        {
            // ⚠️ **La pièce se reconnaît à son NOM, pas à sa forme.** Deux pièces différentes
            // peuvent avoir des boîtes de même taille à un pixel près, et l'appariement par forme
            // rendait des translations incohérentes d'une pièce à l'autre — mesuré : −25, 1459,
            // 2624, 3893 sur cinq pièces de la même page. Le nom, lui, ne se trompe pas ; le
            // dessin lui ajoute un suffixe quand il doit distinguer deux objets de même nom, d'où
            // les essais `Manche`, `Manche_1`, `Manche_2`…
            QVector<QPointF> candidates;
            for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece"));
                 !element.isNull(); element = element.nextSiblingElement(QStringLiteral("piece")))
            {
                if (!referenceLabel.isEmpty()
                    && element.attribute(QStringLiteral("size")) != referenceLabel)
                {
                    continue;
                }
                // La pièce par son IDENTIFIANT, quand le dessin le porte : c'est la seule clé qui
                // distingue la vue d'un membre de la copie que son groupe en contient.
                const QString pieceId = element.attribute(QStringLiteral("id"));
                QRectF drawn = drawnInk.value(QStringLiteral("#") + pieceId);
                if (drawn.isNull())
                {
                    const QString key = Slug(element.attribute(QStringLiteral("name")));
                    for (int suffix = 0; suffix < 10 && drawn.isNull(); ++suffix)
                    {
                        const QString id =
                            suffix == 0 ? key : key + QStringLiteral("_%1").arg(suffix);
                        if (drawnInk.contains(id))
                        {
                            drawn = drawnInk.value(id);
                        }
                    }
                }
                if (drawn.isNull())
                {
                    continue;
                }
                QPointF align(0.0, 0.0);
                const QVector<double> shift = Numbers(element.attribute(QStringLiteral("alignDelta")));
                if (shift.size() >= 2)
                {
                    align = QPointF(shift.at(0), shift.at(1));
                }
                GrowingBox box;
                for (const QPolygonF &polygon : InkPaths(element.attribute(QStringLiteral("outlineInk"))))
                {
                    for (const QPointF &point : polygon)
                    {
                        box.Add(point.x(), point.y());
                    }
                }
                const QRectF own = box.Rect();
                if (own.isNull())
                {
                    continue;
                }
                const QPointF found(drawn.left() - (own.left() + align.x()),
                                    drawn.top() - (own.top() + align.y()));
                drawnShift.insert(QStringLiteral("#") + pieceId, found);
                candidates.append(found);
            }

            // La valeur la plus fréquente : la marge de page, la même pour toute la feuille.
            int best = -1;
            int bestCount = 0;
            for (int i = 0; i < candidates.size(); ++i)
            {
                int count = 0;
                for (const QPointF &other : candidates)
                {
                    if (std::abs(other.x() - candidates.at(i).x()) < 1.0
                        && std::abs(other.y() - candidates.at(i).y()) < 1.0)
                    {
                        ++count;
                    }
                }
                if (count > bestCount)
                {
                    bestCount = count;
                    best = i;
                }
            }
            if (best >= 0)
            {
                drawnShift.clear();
                drawnShift.insert(QStringLiteral("*"), candidates.at(best));
            }
        }
        }
    }

    for (QDomElement group = pieces.firstChildElement(QStringLiteral("group")); !group.isNull();
         group = group.nextSiblingElement(QStringLiteral("group")))
    {
        const QString id = group.attribute(QStringLiteral("id"));
        PieceDecor one;
        const QDomElement element = group.firstChildElement(QStringLiteral("decor"));
        if (!ReadDecor(element, &one, cloth == Cloth::AsDrawn))
        {
            continue;
        }

        // Into the frame the outlines are written in, by the very translation the canvas applies.
        const QPointF origin = groupOrigin.value(id);
        if (!origin.isNull())
        {
            one.content = QStringLiteral("<g transform=\"translate(%1,%2)\">")
                              .arg(QString::number(origin.x(), 'f', 3),
                                   QString::number(origin.y(), 'f', 3))
                          + one.content + QStringLiteral("</g>");
            one.box.translate(origin);
        }

        groupDecor.insert(id, one);
        groupDecorSource.insert(id, element);
    }

    // ── Le groupe est-il dessiné ENTIER ? ───────────────────────────────────────────────────────
    //
    // Décision de Sophie (24-10-2026) : le groupe entier l'emporte — un cartouche, une droit-fil pour
    // deux pièces — mais un membre affiché SEUL porte ses propres enfants. « Entier » se mesure ici,
    // une fois : tous les membres que le payload nomme pour ce groupe sont-ils gardés ?
    //
    // ⚠️ **Et c'est le seul endroit qui puisse le dire.** Le lecteur ne reçoit qu'une liste de pièces
    // cochées ; le groupe n'existe que par l'attribut `group` de ses membres, et sa composition est
    // dans le payload, pas dans la sélection.
    QHash<QString, bool> groupWhole;
    for (QDomElement element = pieces.firstChildElement(QStringLiteral("piece"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("piece")))
    {
        const QString groupId = element.attribute(QStringLiteral("group"));
        if (groupId.isEmpty())
        {
            continue;
        }

        const QString name = element.attribute(QStringLiteral("name"));
        const bool memberKept = kept.contains(name) || kept.contains(Slug(name));
        groupWhole[groupId] = groupWhole.value(groupId, true) && memberKept;
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

            // ── La quantité, dans la matière demandée ──────────────────────────────────────────────
            //
            // ⚠️ **0 n'est pas « une fois », c'est « pas du tout »** — et la figure se lit dans la
            // matière du plan, jamais dans une autre : une pièce taillée en doublure seulement n'a
            // rien à faire dans le plan de tissu, et réciproquement. Aucune ne se replie sur l'autre.
            //
            // Un plan de coupe lit donc la quantité de SA matière ; une impression ne lit rien —
            // `Cloth::AsDrawn` pose chaque pièce une fois et la matière ne la concerne pas — et garde
            // le test d'hier, sur le tissu, pour que son dessin soit exactement le même qu'avant.
            const int cutFabric = element.attribute(QStringLiteral("cutFabric")).toInt();
            const int cutLining = element.attribute(QStringLiteral("cutLining")).toInt();
            const int cut = (cloth == Cloth::AsDrawn || material == Material::Fabric) ? cutFabric
                                                                                     : cutLining;
            if (cut <= 0)
            {
                // Not cut in this cloth at all — a piece somebody left blank in the cartouche, or a
                // piece that is cut in the other cloth, which is not « once » but « not at all », and
                // takes no part in this plan.
                continue;
            }

            const QVector<double> axis = Numbers(element.attribute(QStringLiteral("foldAxis")));
            const QVector<double> grainLine =
                Numbers(element.attribute(QStringLiteral("grainLine")));
            // Its own children, or its group's — the payload states which by carrying them (or not).
            // Filed under THIS size, because the payload seats the drawing on each size separately.
            // Where the block actually came from: the piece's own `<decor>`, or its group's. A twin is
            // re-laid from this element, and a member's own block and its group's are not the same
            // drawing. Declared here because the twin is built after this scope closes.
            QDomElement decorSource = element.firstChildElement(QStringLiteral("decor"));
            // What the block has to travel before it is in the piece's frame: the group's own origin
            // when the block is the group's, and nothing when it is the piece's.
            QPointF decorShift;
            // Ce qu'il faut ajouter à l'ÉLÉMENT du décor (et non au bloc déjà posé) pour l'amener
            // dans le repère des contours : l'origine du groupe, quand le bloc est le sien.
            QPointF groupElementShift;

            // ── Le décor du DESSIN, quand le payload le porte ────────────────────────────────────
            //
            // Il est écrit une fois pour toutes les tailles, dans le repère de la scène : la
            // translation mesurée plus haut le ramène dans celui des pièces, et l'écart
            // d'alignement ci-dessous fait le reste — c'est déjà ce que le lecteur applique au
            // contour. Aucune reprojection par taille, donc, et rien à dériver.
            {
                const QString drawnKey = Slug(name);
                // Uniquement par identifiant : sans lui, on ne devine pas (c'est ce qui a produit
                // les appariements faux de la journée — « Manche » désigne deux objets).
                auto wrapper = drawnDecor.constFind(QStringLiteral("#") + element.attribute(QStringLiteral("id")));
                if (wrapper == drawnDecor.cend())
                {
                    for (int suffix = 1; suffix < 10 && wrapper == drawnDecor.cend(); ++suffix)
                    {
                        wrapper = drawnDecor.constFind(drawnKey + QStringLiteral("_%1").arg(suffix));
                    }
                }
                auto shift = drawnShift.constFind(QStringLiteral("#") + element.attribute(QStringLiteral("id")));
                if (shift == drawnShift.cend())
                {
                    shift = drawnShift.constFind(QStringLiteral("*"));
                }
                if (wrapper != drawnDecor.cend() && shift != drawnShift.cend())
                {
                    const QPointF back = shift.value();
                    QDomElement carried = payload.createElement(QStringLiteral("decor"));
                    QDomElement moved = payload.createElement(QStringLiteral("g"));
                    moved.setAttribute(QStringLiteral("transform"),
                                       QStringLiteral("translate(%1,%2)")
                                           .arg(QString::number(-back.x(), 'f', 3),
                                                QString::number(-back.y(), 'f', 3)));
                    for (QDomElement node = wrapper.value().firstChildElement(); !node.isNull();
                         node = node.nextSiblingElement())
                    {
                        moved.appendChild(node.cloneNode(true));
                    }
                    carried.appendChild(moved);
                    decorSource = carried;
                    // ⚠️ **Rien à ajouter : la translation mesurée plus haut a déjà ramené le
                    // dessin dans le repère des pièces, écart d'alignement compris.** La retrancher
                    // une seconde fois décalait tout de 219 px en x à la taille 46 — mesuré.
                    decorShift = QPointF();
                }
            }
            // L'assise du décor de cette pièce POUR CETTE TAILLE — la même pour elle et pour son
            // jumeau, qui est le même dessin retourné. Calculée avec le décor, plus bas.
            QPointF decorSeatShift;
            {
                PieceDecor one;
                const QString groupId = element.attribute(QStringLiteral("group"));
                // ── Qui gagne, du membre ou du groupe ───────────────────────────────────────────
                //
                // **Le groupe entier l'emporte** (décision de Sophie, 24-10-2026) : le groupe est un
                // dessin, il porte un cartouche et une droit-fil pour ses membres, et c'est ce que
                // l'impression montre. Un membre qui en porte un à lui ne le fait valoir que lorsqu'il
                // est affiché SEUL — c'est-à-dire quand son groupe n'est pas gardé en entier.
                //
                // Sans ce test, les enfants de chaque membre prendraient la place de ceux du groupe
                // dès qu'Ostara les écrit, et un groupe imprimé entier porterait deux cartouches.
                //
                // ⚠️ **Et une règle d'IMPRESSION, pas de plan de coupe.** Un plan de coupe
                // individualise les membres — chacun est une pièce à poser, à côté de l'autre — donc
                // le groupe n'y gagne JAMAIS, même quand ses membres sont tous retenus : ce que la
                // pièce porte, c'est son propre dessin. Le groupe n'est qu'un dessin pour la sortie
                // qui reproduit le document tel qu'il se lit, membres côte à côte.
                //
                // ⚠️ **Et le groupe garde son bloc, même depuis la version 5.** Le payload porte
                // toujours `<group id="679">` avec SON `<decor>` (vérifié : deux groupes dans
                // `32-46-160-A0Roll.pdf`, chacun avec le sien), et c'est ce dessin-là qu'un groupe
                // complet montre — un cartouche, un droit-fil, une fois. Un membre n'existe seul que
                // lorsque le groupe ne l'est pas.
                const bool groupWins = cloth == Cloth::AsDrawn && groupWhole.value(groupId);
                const QDomElement own =
                    (!groupId.isEmpty() && groupWins) ? QDomElement() : decorSource;
                if (!ReadDecor(own, &one, cloth == Cloth::AsDrawn) && cloth == Cloth::AsDrawn)
                {
                    one = groupDecor.value(groupId);
                    decorSource = groupDecorSource.value(groupId);
                    // ⚠️ **Le BLOC est déjà dans le repère des contours** : l'origine du groupe y a été
                    // portée une fois, quand le groupe a été lu. La rajouter ici la portait deux fois,
                    // et c'est ce qui envoyait le bloc de « Manche » à 2 900 px de ses pièces sur le
                    // print — mesuré. Rien à ajouter, donc.
                    decorShift = QPointF();
                    // L'ÉLÉMENT, lui, n'y est pas : le jumeau se re-lie au DESSIN, qui est resté dans
                    // le repère du groupe. Son origine voyage donc séparément.
                    groupElementShift = groupOrigin.value(groupId);
                }
                if (!one.content.isEmpty())
                {
                    // ⚠️ **Le décor prend l'écart d'alignement, comme la pièce qu'il décore.** Le
                    // plan pose la pièce à `contour + alignDelta` : le décor écrit à côté du contour
                    // doit donc subir le même écart, sinon il reste où le contour était avant — 239 px
                    // à gauche pour « Devant », et vers le haut pour les pièces dont l'écart est
                    // vertical. Mesuré sur la trace du plan, pièce par pièce.
                    // ⚠️ **Sur une pièce à pli, le décor est accroché au PLI, et à l'axe seul.**
                    // C'est l'axe du pli qui est le repère : il ne bouge pas d'une taille à l'autre,
                    // tandis que le coin de la pièce s'en va de 194 px entre la 32 et la 46. Ancrer au
                    // coin faisait glisser le cartouche le long du bord. Le bas de la pièce complète
                    // l'assise en y, pour la même raison.
                    // ⚠️ **Les autres pièces n'ont pas de meilleure assise** : le payload n'écrit
                    // qu'UN décor, dans le repère de la référence, et les tailles ne se superposent
                    // pas. Il leur reste l'écart d'alignement, donc un résidu qui suit la gradation —
                    // mesuré jusqu'à 227 px en x et 42 px en y selon la pièce. Le corriger demande un
                    // décor **par taille**, écrit par Ostara (§13) : aucun ancrage ne peut le deviner
                    // ici.
                    //
                    // ⚠️ **Et depuis la version 5, Ostara l'écrit.** Le bloc est alors DÉJÀ à la place
                    // de sa taille, dans le même repère que son `outline` : il ne prend que l'écart
                    // d'alignement, comme le contour, et l'ancrage au pli ci-dessous le SUR-décalerait
                    // d'exactement la croissance de la pièce. Il ne vaut donc que pour la 4 et avant.
                    // ⚠️ **Et l'ancrage au pli a été retiré : il compensait un écart qui n'existe
                    // pas.** Il ajoutait `pli(de cette taille) − pli(référence)` PAR-DESSUS l'écart
                    // d'alignement, alors que celui-ci contient déjà la croissance : mesuré sur
                    // « Devant x1 pli », il produisait 529,12 à la taille 46 là où le décalage correct
                    // est 0 — et il ne retombait juste qu'à la référence, où il redonne alignDelta par
                    // construction. C'est ce qui lui donnait l'air de marcher.
                    // ⚠️ **Le décor va là où la pièce est, et c'est tout.**
                    //
                    // Le dessin est le même octet pour les huit tailles (mesuré : un seul md5), écrit
                    // dans le repère de la taille de RÉFÉRENCE. La pièce, elle, n'est pas au même
                    // endroit selon la taille :
                    //
                    //   · pièce ordinaire — `outline + alignDelta` est INVARIANT (mesuré sur
                    //     « Parementure manche » : x[4245,7 ; 4719,8] dans les huit tailles) ;
                    //   · pièce AU PLI — il se déplace (mesuré sur « Devant » : le coin passe de
                    //     1246,8 en 32 à 911,6 en 46, soit −37,8 px par taille), parce que la moitié
                    //     grandit depuis son pli et que l'écart d'alignement ne la ramène pas.
                    //
                    // Une translation constante est donc juste pour l'une et fausse pour l'autre — et
                    // c'est exactement ce qui se voyait : les pièces à pli décalaient, les autres non.
                    // La seule valeur juste pour les deux est celle qui joint le coin de la référence
                    // au coin de la pièce DE CETTE TAILLE : le décor est le même dessin, posé sur la
                    // pièce que l'on regarde.
                    QPointF currentCorner = alignDelta;
                    if (!contour.isEmpty())
                    {
                        double x0 = contour.first().x();
                        double y0 = contour.first().y();
                        for (const QPointF &point : contour)
                        {
                            x0 = std::min(x0, point.x());
                            y0 = std::min(y0, point.y());
                        }
                        currentCorner = QPointF(x0, y0);
                    }
                    //
                    // ⚠️ **Et depuis le 25-10-2026, c'est l'écart de la RÉFÉRENCE, pas celui de la
                    // pièce** (décision de Sophie : « les enfants se positionnent par rapport à la
                    // taille de référence », la 32). Le bloc est écrit une fois, à la place de la 32,
                    // et il ne bouge pas d'une taille à l'autre — c'est la PIÈCE qui grandit autour
                    // de lui, depuis le pli. L'ancrer au coin de chaque taille lui faisait suivre
                    // cette croissance : mesuré sur « Devant x1 pli », 174,6 px d'écart à la 40 et
                    // 335,2 px à la 46, exactement la croissance du coin (1246,8 → 1072,2 → 911,6).
                    // C'est ce qui décalait le plan de coupe, et le print le montrait déjà juste
                    // parce qu'il n'a jamais qu'une seule assise : celle de la référence.
                    QPointF decorShiftHere = decorShift;
                    decorShiftHere += decorShared
                                          ? referenceAlignDelta.value(name, currentCorner)
                                          : alignDelta;
                    // Retenue pour le jumeau : le sien est ce MÊME dessin, retourné. Lui donner une
                    // autre assise le décollerait de la pièce qu'il double.
                    decorSeatShift = decorShiftHere;
                    if (!decorShiftHere.isNull())
                    {
                        one.content = QStringLiteral("<g transform=\"translate(%1,%2)\">")
                                          .arg(QString::number(decorShiftHere.x(), 'f', 3),
                                               QString::number(decorShiftHere.y(), 'f', 3))
                                      + one.content + QStringLiteral("</g>");
                        one.box.translate(decorShiftHere);
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
            pose.cutLining = cutLining;
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

            // ⚠️ **L'angle de la pièce vient de sa LIGNE, quand elle en porte une.**
            //
            // `grain` et `grainLine` sont deux écritures d'un même fait, et une pièce dont le
            // droit-fil n'est pas à 90° les a montrées en désaccord : « Parementure manche x2 +
            // entoilage » du patron d'essai porte `grain="135.000"` à côté d'une `grainLine` à
            // **45°** et d'un tracé `ostara:grain` à **45°** — l'angle était le MIROIR de la ligne
            // (`180 − 45`), et un plan qui épinglait la pièce sur lui la posait d'un quart de tour
            // en travers du droit-fil.
            //
            // À 90°, un miroir vertical ne change rien (`180 − 90 = 90`) : le défaut est invisible
            // sur toutes les pièces du patron sauf une, ce qui explique qu'il n'ait jamais été vu.
            //
            // La ligne, elle, est écrite MAPPÉE — comme le contour qu'elle traverse — et le tracé du
            // décor vient de la même géométrie : deux sources sur trois disent 45°. On garde celle
            // qui s'accorde avec le dessin. L'absence d'angle reste une absence : une pièce sans
            // `grain` n'est pas épinglée par sa ligne.
            if (pose.grainKnown && pose.hasGrainLine)
            {
                pose.grain = QLineF(pose.grainA, pose.grainB).angle();
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
            int quantity = cloth == Cloth::AsDrawn ? 1 : cut;
            if (cloth == Cloth::Folded && !onFold)
            {
                quantity = (cut + 1) / 2;
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

                    // ⚠️ **The children follow too, and each twin carries its OWN block.** They cannot
                    // share the piece's: the drawing's children are hung in the piece's frame, which a
                    // reflection of the outline never reaches — so the cartouche, the miniature and the
                    // annotations would stay on the side the outline has just left. Filed under the
                    // twin's key, because a pose and its twin are two drawings of one piece and the
                    // plan looks a block up by the key it lays.
                    PieceDecor mirroredOne;
                    // ⚠️ **L'écart du jumeau est celui de la RÉFÉRENCE, pas celui de la taille.**
                    // Son dessin est le décor de la référence retourné une fois pour toutes ; le
                    // poser avec l'écart de la taille demandée le déplaçait de la différence — nulle
                    // à la référence, et de plus en plus grande au-dessus, vers la droite.
                    //
                    // ⚠️ **Et depuis la version 5, son dessin est celui de SA taille** : il prend donc
                    // l'écart de sa taille, exactement comme le bloc dont il est le miroir.
                    const QPointF twinShift =
                        decorShared ? decorSeatShift
                                    : (decorPerSize ? alignDelta
                                                    : referenceAlignDelta.value(name, alignDelta))
                                          + decorShift + groupElementShift;
                    // ⚠️ **Le jumeau est le décor de la POSE, retourné — pas l'élément brut.**
                    //
                    // `decorSource` est le dessin de la référence : le retourner puis le décaler
                    // redonne une boîte qui ne bouge plus quand la pièce grandit (mesuré : décor fixe
                    // à x 1246,8 de la 32 à la 46, alors que le bord de la pièce s'en allait de 37,8 px
                    // par taille). Le jumeau décalait donc dès qu'on quittait la taille de référence.
                    //
                    // Le bloc de la pose est DÉJÀ posé pour cette taille (`one`, translation comprise).
                    // On le prend tel quel et on le retourne : le miroir suit alors la pièce comme la
                    // pose suit la sienne.
                    const PieceDecor placedDecor = size.decor.value(name);
                    QDomDocument placedDocument;
                    const bool havePlaced =
                        !placedDecor.content.isEmpty()
                        && placedDocument.setContent(
                            QStringLiteral("<decor xmlns:ostara=\"urn:ostara\">%1</decor>")
                                .arg(placedDecor.content));
                    bool haveTwin = false;
                    if (havePlaced)
                    {
                        haveTwin = ReadMirroredDecor(placedDocument.documentElement(), QPointF(),
                                                     axisX, &mirroredOne);
                    }
                    else
                    {
                        haveTwin = ReadMirroredDecor(decorSource, twinShift, axisX, &mirroredOne);
                    }
                    if (haveTwin)
                    {
                        // ⚠️ **Rien à ajouter après le miroir.** Il est pris autour du centre de la
                        // pièce, qui est aussi le centre du jumeau : le bloc se retrouve donc du côté
                        // opposé, à la même distance du bord que l'original — ce qui est exactement
                        // le dessin du jumeau.
                        //
                        // Le déplacement que l'on ajoutait ici (le coin du jumeau moins le coin de la
                        // référence) était la croissance de la taille, appliquée une SECONDE fois :
                        // le miroir la porte déjà, par l'axe qu'il prend. Mesuré sur
                        // « Devant x1 pli-m » : 37,8 px de trop à la 34 — une taille de croissance,
                        // et le dessin sortait de sa pièce du côté du pli.
                        size.decor.insert(twin.key, mirroredOne);
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
            // ⚠️ **Une taille sans pièce de doublure n'est pas un défaut : elle n'a pas de plan de
            // doublure, et c'est tout.** Le tissu, lui, ne se tait pas — un plan de tissu vide est une
            // erreur de lecture du PDF. La taille est tout de même rendue, vide, pour que la doublure
            // garde l'index de sa taille : deux listes appariées se lisent par le même rang.
            if (material == Material::Lining)
            {
                out.append(size);
                continue;
            }

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
