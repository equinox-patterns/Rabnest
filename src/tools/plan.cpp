/************************************************************************
 **
 **  @file   plan.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  A cutting plan without a click — the figures, and no file.
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

#include "core/cutplan.h"
#include "core/cutplanrequest.h"
#include "core/importeddocument.h"
#include "core/pattern.h"

#include <clipper2/clipper.h>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QElapsedTimer>
#include <QPolygonF>
#include <QTransform>
#include <QSvgRenderer>
#include <QTextStream>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/// Le plan tel que le canvas le dessine, rendu dans une image — pour REGARDER au lieu de deviner.
///
/// Mêmes formules que `WorkspacePage`, à la lettre : l'encre de la pose passée par `PlacedPoint`, puis
/// le décor de la pièce par la même transformation. C'est un outil de diagnostic, il ne remplace rien.
auto RenderPlan(const CutPlan &plan, const QString &path) -> bool
{
    const double scale = 0.25;  // 96 dpi vers une image regardable
    const int width = static_cast<int>(plan.binWidthPx * scale);
    const int height = static_cast<int>(plan.usedHeightPx * scale);
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(scale, scale);

    QPen border(QColor(0xD6, 0xD6, 0xD6));
    border.setCosmetic(true);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx));

    QPen foldPen(QColor(0x9A, 0x9A, 0x9A), 0.0, Qt::DashLine);
    foldPen.setCosmetic(true);
    for (const FoldZone &zone : plan.zones)
    {
        painter.setPen(foldPen);
        painter.drawRect(zone.rect);
        const double x = zone.foldOnTheLeft ? zone.rect.left() : zone.rect.right();
        painter.setPen(QPen(QColor(0x7A, 0x7A, 0x7A), 0.0, Qt::DashDotLine));
        painter.drawLine(QLineF(x, zone.rect.top(), x, zone.rect.bottom()));
    }

    QPen piecePen(QColor(0x2B, 0x2B, 0x2B));
    piecePen.setCosmetic(true);

    for (int index = 0; index < plan.poses.size() && index < static_cast<int>(plan.placements.size());
         ++index)
    {
        const CutPiece &pose = plan.poses.at(index);
        const Placement &placement = plan.placements.at(static_cast<std::size_t>(index));
        const QPointF origin = plan.origins.at(index);

        QPainterPath shape;
        for (const QPolygonF &polygon : pose.ink)
        {
            QPolygonF placed;
            placed.reserve(polygon.size());
            for (const QPointF &point : polygon)
            {
                placed.append(PlacedPoint(point, placement, origin));
            }
            shape.addPolygon(placed);
        }

        painter.setPen(piecePen);
        painter.setBrush(QColor(0xF7, 0xF5, 0xF2));
        painter.drawPath(shape);

        const PieceDecor decor = plan.decor.value(pose.name);
        if (decor.content.isEmpty())
        {
            continue;
        }

        const QRectF box = decor.box;
        QString svg = QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                     "xmlns:ostara=\"urn:ostara\" width=\"%1\" height=\"%2\" "
                                     "viewBox=\"%3 %4 %1 %2\">")
                          .arg(QString::number(box.width(), 'f', 3),
                               QString::number(box.height(), 'f', 3),
                               QString::number(box.left(), 'f', 3),
                               QString::number(box.top(), 'f', 3));
        svg += decor.content;
        svg += QStringLiteral("</svg>");

        QSvgRenderer renderer(svg.toUtf8());
        if (!renderer.isValid())
        {
            continue;
        }

        painter.save();
        QTransform transform;
        transform.translate(placement.x, placement.y);
        transform.rotate(placement.rotation);
        transform.translate(box.left() - origin.x(), box.top() - origin.y());
        painter.setTransform(transform, /*combine=*/true);
        renderer.render(&painter, QRectF(box.left(), box.top(), box.width(), box.height()));
        painter.restore();
    }

    painter.end();
    return image.save(path);
}
}  // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();

    if (arguments.size() < 2)
    {
        QTextStream(stderr) << "usage: rabnest_plan <pdf> [laize_cm] [folded] [oneway]\n";
        return 2;
    }

    // A payload SVG is accepted outright: a fixture states a piece, and the plan is what is being
    // measured — not the PDF reader.
    ImportedDocument document;
    if (arguments.at(1).endsWith(QStringLiteral(".svg")))
    {
        QFile file(arguments.at(1));
        if (!file.open(QIODevice::ReadOnly))
        {
            QTextStream(stderr) << "cannot open " << arguments.at(1) << "\n";
            return 1;
        }
        document = ImportedDocument::FromPayload(file.readAll(), arguments.at(1));
    }
    else
    {
        document = ImportedDocument::Read(arguments.at(1));
    }
    if (!document.isValid() || document.kind() != ImportedDocument::Kind::Ostara)
    {
        QTextStream(stderr) << "not an Ostara PDF: " << document.error() << "\n";
        return 1;
    }

    CutPlanRequest request;
    if (arguments.size() > 2)
    {
        request.widthCm = arguments.at(2).toDouble();
    }
    request.foldedFabric = arguments.contains(QStringLiteral("folded"));
    request.oneWay = arguments.contains(QStringLiteral("oneway"));

    // Anything that is not a number and not one of the two flags names a piece. Left empty, every
    // piece is asked for — which is what a plan needs; a single piece is how one rule is looked at.
    QStringList only;
    for (int index = 3; index < arguments.size(); ++index)
    {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("folded") || argument == QStringLiteral("oneway"))
        {
            continue;
        }
        bool isNumber = false;
        argument.toDouble(&isNumber);
        if (!isNumber)
        {
            only.append(argument);
        }
    }

    QStringList sizes;
    for (const ImportedLayer &layer : document.layers())
    {
        if (!layer.decor)
        {
            sizes.append(layer.id);
        }
    }

    QTextStream out(stdout);
    out << "laize " << request.BinWidthCm() << " cm"
        << (request.foldedFabric ? " (plié)" : "") << (request.oneWay ? " (sens unique)" : "")
        << "\n";

    for (const QString &sizeId : sizes)
    {
        QStringList pieces = document.Pieces({sizeId});
        if (arguments.contains(QStringLiteral("list")))
        {
            out << "  " << sizeId << " pieces: " << pieces.join(QStringLiteral(" | ")) << "\n";
            continue;
        }
        if (!only.isEmpty())
        {
            pieces = only;
        }

        QVector<CutSize> loaded;
        QString error;
        if (!Pattern::Load(document, {sizeId}, pieces,
                          request.foldedFabric ? Cloth::Folded : Cloth::Flat, loaded, &error))
        {
            out << "  " << sizeId << " : " << error << "\n";
            continue;
        }

        for (CutSize size : loaded)
        {
            if (!size.decor.isEmpty())
            {
                int shown = 0;
                for (auto it = size.decor.cbegin(); it != size.decor.cend() && shown < 3;
                     ++it, ++shown)
                {
                    out << "    décor " << it.key() << " : boîte "
                        << it.value().box.width() << " × " << it.value().box.height()
                        << ", contenu " << it.value().content.size() << " o\n";
                }
            }
            else
            {
                out << "    décor : AUCUN\n";
            }

            if (arguments.contains(QStringLiteral("nodecor")))
            {
                size.decor.clear();
            }

            QElapsedTimer clock;
            clock.start();
            const CutPlan plan = CutPlan::Generate(size, request, size.decor);
            out << "      temps " << clock.elapsed() << " ms\n";
            if (!plan.ok)
            {
                out << "  " << size.label << " : ECHEC " << plan.error << "\n";
                continue;
            }

            // Un PNG par taille, pour regarder le plan au lieu de le décrire.
            for (const QString &argument : arguments)
            {
                if (!argument.startsWith(QStringLiteral("--png=")))
                {
                    continue;
                }
                const QString base = argument.mid(6);
                const QString file = base + QStringLiteral("-%1.png").arg(size.label);
                out << "      image " << file << (RenderPlan(plan, file) ? "" : " (ECHEC)") << "\n";
            }

            // The two things a plan cannot get wrong, measured on what the engine returned: no two
            // pieces overlap, and none of them leaves the cloth.
            constexpr double kScale = 1000000.0;
            std::vector<Clipper2Lib::Path64> placed;
            placed.reserve(plan.placements.size());
            bool outside = false;
            for (std::size_t index = 0; index < plan.placements.size(); ++index)
            {
                const Placement &placement = plan.placements.at(index);
                const QPointF origin = plan.origins.at(static_cast<int>(index));

                // The same function the canvas draws with: what is measured here is what is seen.
                const QPolygonF outline = PlacedOutline(
                    plan.footprint.polygons.at(index), placement, origin);

                Clipper2Lib::Path64 path;
                for (const QPointF &point : outline)
                {
                    path.push_back({static_cast<int64_t>(std::llround(point.x() * kScale)),
                                    static_cast<int64_t>(std::llround(point.y() * kScale))});
                    if (point.x() < -0.5 || point.x() > plan.binWidthPx + 0.5 || point.y() < -0.5)
                    {
                        outside = true;
                    }
                }
                placed.push_back(path);
            }

            int overlaps = 0;
            for (std::size_t a = 0; a < placed.size(); ++a)
            {
                for (std::size_t b = a + 1; b < placed.size(); ++b)
                {
                    const Clipper2Lib::Paths64 common =
                        Clipper2Lib::Intersect({placed.at(a)}, {placed.at(b)},
                                               Clipper2Lib::FillRule::NonZero);
                    if (!common.empty() && std::abs(Clipper2Lib::Area(common)) > kScale)
                    {
                        ++overlaps;
                    }
                }
            }

            // A piece cut on the fold is glued to the fold, and the fold is the left wall of the
            // column: its leftmost point has to sit on zero. Anything else is the piece laid in the
            // middle of the cloth, which is the defect this whole route exists for.
            for (std::size_t index = 0; index < plan.poses.size() && index < placed.size(); ++index)
            {
                if (!plan.poses.at(static_cast<int>(index)).onFold)
                {
                    continue;
                }

                int64_t low = placed.at(index).front().x;
                int64_t high = low;
                for (const auto &point : placed.at(index))
                {
                    low = std::min(low, point.x);
                    high = std::max(high, point.x);
                }
                // The proof that matters: the fold LINE — not just some edge — has to land on a
                // fold of the cloth after the transformation the drawing uses.
                const CutPiece &pose = plan.poses.at(static_cast<int>(index));
                const Placement &placement = plan.placements.at(index);
                const QPointF origin = plan.origins.at(static_cast<int>(index));
                const QPointF a = PlacedPoint(pose.foldA, placement, origin);
                const QPointF b = PlacedPoint(pose.foldB, placement, origin);

                const double cm = kScale * (96.0 / 2.54);
                out << "      ligne de pli : x = "
                    << (a.x() - b.x()) / (96.0 / 2.54) << " d'écart, à "
                    << a.x() / (96.0 / 2.54) << " cm\n";

                out << "      au pli : " << plan.poses.at(static_cast<int>(index)).name
                    << " — x de " << static_cast<double>(low) / cm << " à "
                    << static_cast<double>(high) / cm << " cm"
                    << " (bord droit du tissu : " << plan.binWidthPx / (96.0 / 2.54) << " cm)\n";
            }

            // Où le décor se situe par rapport à la pièce, dans le même repère : boîte du polygone
            // contre boîte du dessin. C'est le seul moyen de voir un décalage sans regarder l'écran.
            for (int index = 0; index < plan.poses.size(); ++index)
            {
                const CutPiece &pose = plan.poses.at(index);
                const PieceDecor decor = plan.decor.value(pose.name);
                if (decor.content.isEmpty())
                {
                    continue;
                }

                const Clipper2Lib::Path64 &poly = plan.footprint.polygons.at(
                    static_cast<std::size_t>(index));
                double px0 = 1e18, py0 = 1e18, px1 = -1e18, py1 = -1e18;
                for (const auto &pt : poly)
                {
                    const double x = static_cast<double>(pt.x) / kScale;
                    const double y = static_cast<double>(pt.y) / kScale;
                    px0 = std::min(px0, x); px1 = std::max(px1, x);
                    py0 = std::min(py0, y); py1 = std::max(py1, y);
                }
                const QRectF pieceRect(px0, py0, px1 - px0, py1 - py0);
                const QRectF inside = pieceRect.intersected(decor.box);
                const double ratio = decor.box.width() * decor.box.height() > 0.0
                                         ? (inside.width() * inside.height())
                                               / (decor.box.width() * decor.box.height())
                                         : 0.0;
                // Ce que le canevas affichera vraiment : la boîte du décor passée par la même
                // transformation, comparée à la pièce telle qu'elle est posée.
                const QPointF origin = plan.origins.at(static_cast<int>(index));
                const Placement &placement = plan.placements.at(index);
                const QPolygonF placedPiece =
                    PlacedOutline(plan.footprint.polygons.at(static_cast<std::size_t>(index)),
                                  placement, origin);
                const QRectF placedRect = placedPiece.boundingRect();

                const QPointF pc((px0 + px1) / 2.0, (py0 + py1) / 2.0);
                const QPointF dc = decor.box.center();

                QTransform t;
                t.translate(placement.x, placement.y);
                t.rotate(placement.rotation);
                t.translate(decor.box.left() - origin.x() + pc.x() - dc.x(),
                            decor.box.top() - origin.y() + pc.y() - dc.y());

                const QRectF mapped = t.mapRect(decor.box);
                const QRectF common = mapped.intersected(placedRect);
                const double onScreen = mapped.width() * mapped.height() > 0.0
                                            ? common.width() * common.height()
                                                  / (mapped.width() * mapped.height())
                                            : 0.0;
                out << "      ECRAN " << pose.name << " : " << int(onScreen * 100.0) << "%\n";

                out << "      CONTIENT " << pose.name << " : " << int(ratio * 100.0) << "%\n";

                out << "      decor " << pose.name
                    << " : pièce x[" << px0 << "," << px1 << "] y[" << py0 << "," << py1 << "]"
                    << " | dessin x[" << decor.box.left() << "," << decor.box.right()
                    << "] y[" << decor.box.top() << "," << decor.box.bottom() << "]\n";
            }

            // Ce que le renderer croit devoir dessiner, contre ce que la boîte dit : c'est là que se
            // voit un facteur d'échelle que personne n'a demandé.
            for (int index = 0; index < plan.poses.size(); ++index)
            {
                const PieceDecor decor = plan.decor.value(plan.poses.at(index).name);
                if (decor.content.isEmpty())
                {
                    continue;
                }

                QString svg = QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                             "width=\"%1\" height=\"%2\" viewBox=\"%3 %4 %1 %2\">")
                                  .arg(QString::number(decor.box.width(), 'f', 3),
                                       QString::number(decor.box.height(), 'f', 3),
                                       QString::number(decor.box.left(), 'f', 3),
                                       QString::number(decor.box.top(), 'f', 3));
                svg += decor.content;
                svg += QStringLiteral("</svg>");

                QSvgRenderer renderer(svg.toUtf8());
                out << "      svg " << plan.poses.at(index).name
                    << " valide=" << renderer.isValid()
                    << " defaultSize=" << renderer.defaultSize().width() << "×"
                    << renderer.defaultSize().height()
                    << " boîte=" << decor.box.width() << "×" << decor.box.height()
                    << " viewBox=" << renderer.viewBoxF().width() << "×"
                    << renderer.viewBoxF().height() << "\n";
                break;
            }

            out << "  " << size.label << " : " << plan.poses.size() << " poses, "
                << plan.usedHeightPx * 2.54 / 96.0 << " cm de tissu, "
                << plan.placements.size() << " placements"
                << ", chevauchements " << overlaps
                << (outside ? ", HORS DU TISSU" : "") << "\n";
        }
    }

    return 0;
}
