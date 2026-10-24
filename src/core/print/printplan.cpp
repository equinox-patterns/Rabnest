/************************************************************************
 **
 **  @file   printplan.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's print run, ported: footprints, bin, engine, grid.
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

#include "core/print/printplan.h"

#include "core/importeddocument.h"
#include "core/print/printmetrics.h"

#include "nesting/Bin.h"
#include "nesting/Engine.h"
#include "nesting/TiledEngine.h"

#include <QDomDocument>
#include <QDomElement>
#include <QHash>
#include <QObject>

#include <algorithm>
#include <cstdint>

namespace
{
/// The engine's fixed point. A pixel is worth 1e6 units, as everywhere in the socle.
constexpr double kScale = 1000000.0;

constexpr double kPxPerCm = 96.0 / 2.54;

//---------------------------------------------------------------------------------------------------------------------
auto ToPath64(const QVector<QPointF> &points) -> Clipper2Lib::Path64
{
    Clipper2Lib::Path64 path;
    path.reserve(static_cast<std::size_t>(points.size()));

    for (const QPointF &point : points)
    {
        path.push_back({static_cast<int64_t>(std::llround(point.x() * kScale)),
                        static_cast<int64_t>(std::llround(point.y() * kScale))});
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
/// The min corner of a polygon, in pixels — what a placement's rotation is measured from.
auto MinCorner(const Clipper2Lib::Path64 &path) -> QPointF
{
    if (path.empty())
    {
        return {};
    }

    int64_t x = path.front().x;
    int64_t y = path.front().y;
    for (const auto &point : path)
    {
        x = std::min(x, point.x);
        y = std::min(y, point.y);
    }

    return QPointF(static_cast<double>(x) / kScale, static_cast<double>(y) / kScale);
}

//---------------------------------------------------------------------------------------------------------------------
/// How a size is drawn, read off the payload's own layer for it.
///
/// The payload IS the plan as Ostara drew it, so the colour and the dash pattern of a size are in
/// it: `<g id="Taille_46">` holds the size's paths, and each carries `stroke` and, when the size
/// has one, `stroke-dasharray`. Nothing is derived — a second palette would be a second plan.
auto LayerStyle(const QDomDocument &payload, const QString &sizeId, QColor &colour,
                QVector<qreal> &dashes, qreal &width) -> bool
{
    if (sizeId.isEmpty())
    {
        return false;
    }

    const QDomElement root = payload.documentElement();
    for (QDomElement group = root.firstChildElement(QStringLiteral("g")); !group.isNull();
         group = group.nextSiblingElement(QStringLiteral("g")))
    {
        if (group.attribute(QStringLiteral("id")) != sizeId)
        {
            continue;
        }

        // One piece group deep: `<g id="Taille_46"><g id="Manche"><path stroke=…/></g>…`. The first
        // path that states a stroke is the size's line — Ostara writes the same one on all of them.
        for (QDomElement piece = group.firstChildElement(); !piece.isNull();
             piece = piece.nextSiblingElement())
        {
            for (QDomElement path = piece.firstChildElement(QStringLiteral("path")); !path.isNull();
                 path = path.nextSiblingElement(QStringLiteral("path")))
            {
                if (!path.hasAttribute(QStringLiteral("stroke")))
                {
                    continue;
                }

                const QColor stroke(path.attribute(QStringLiteral("stroke")));
                if (stroke.isValid())
                {
                    colour = stroke;
                }

                dashes.clear();
                const QStringList parts = path.attribute(QStringLiteral("stroke-dasharray"))
                                              .split(QLatin1Char(' '), Qt::SkipEmptyParts);
                for (const QString &part : parts)
                {
                    bool ok = false;
                    const qreal value = part.toDouble(&ok);
                    if (ok && value >= 0.0)
                    {
                        dashes.append(value);
                    }
                }

                // The weight travels with the colour and the dashes, and for the same reason: it is
                // what the creator drew with. The canvas has a floor of its own and ignores it; a
                // file draws at the size the paper will be.
                bool widthOk = false;
                const qreal stated = path.attribute(QStringLiteral("stroke-width")).toDouble(&widthOk);
                width = widthOk && stated > 0.0 ? stated : 0.0;

                return true;
            }
        }
        return false;
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------------
/// How many sheets a plan prints — the ONE figure the arbitration compares two plans on.
///
/// Read with the same two functions the layout uses: the tiling from ExportArtboards and the paper
/// from ExportSheets. A second count would be a second answer to « combien de feuilles ».
auto PrintedSheets(const std::vector<Placement> &placements,
                   const std::vector<Clipper2Lib::Path64> &polygons, const QVector<QPointF> &origins,
                   const Bin &bin, double binWidthPx, double sheetHeightMm, int columns,
                   const QRectF &reserved) -> int
{
    const double usedHeightPx = Engine::UsedHeight(placements, polygons, bin);
    const ExportArtboards::Grid grid =
        ExportArtboards::Compute(binWidthPx, usedHeightPx, sheetHeightMm, columns);
    if (!grid.valid)
    {
        return 0;
    }

    std::vector<Placement> copy = placements;
    QVector<QPolygonF> ink;
    ink.reserve(static_cast<int>(polygons.size()));
    for (int index = 0; index < static_cast<int>(polygons.size()); ++index)
    {
        const Clipper2Lib::Path64 &polygon = polygons.at(static_cast<std::size_t>(index));
        const Placement &placement = copy.at(static_cast<std::size_t>(index));
        const QPointF origin = origins.at(index);

        const double radians = placement.rotation * 3.14159265358979323846 / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);

        QPolygonF shape;
        shape.reserve(static_cast<int>(polygon.size()));
        for (const auto &point : polygon)
        {
            const double x = static_cast<double>(point.x) / kScale - origin.x();
            const double y = static_cast<double>(point.y) / kScale - origin.y();
            shape.append(QPointF(x * c - y * s + placement.x, x * s + y * c + placement.y));
        }
        ink.append(shape);
    }

    return ExportSheets::Compute(grid, ink, reserved).count;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto PrintPlan::Generate(const ImportedDocument &document, const QStringList &sizeIds,
                         const QStringList &pieceNames, double marginCm, double gapCm,
                         double binWidthPx, int columns, double sheetHeightMm,
                         const QRectF &coverBlockPx, const QString &modelToken,
                         bool plainOutline) -> PrintPlan
{
    PrintPlan plan;
    plan.modelToken = modelToken;
    plan.plainOutline = plainOutline;
    plan.marginCm = marginCm;
    plan.gapCm = gapCm;
    plan.binWidthPx = binWidthPx;
    plan.columns = std::max(columns, 1);
    plan.sheetHeightMm = sheetHeightMm;

    if (binWidthPx <= 0.0)
    {
        plan.error = QObject::tr("The width of the document is zero.");
        return plan;
    }

    // ── Les pièces, telles qu'un run d'impression les voit ──────────────────────────────────────
    //
    // **`AsDrawn`, and not the cloth's fold: a print reproduces the document.** A piece the payload
    // carries folded is printed folded, one it carries whole is printed whole — the print re-forms
    // nothing, and neither takes a twin nor expands a cut quantity. The half form, the twin and the
    // quantity belong to the cutting plan, which is the only output that produces pieces to cut.
    QVector<CutSize> sizes;
    QString error;
    if (!Pattern::Load(document, sizeIds, pieceNames, Cloth::AsDrawn, sizes, &error))
    {
        plan.error = error;
        return plan;
    }

    // ── Ce que le moteur voit : une empreinte par pièce, l'union de ses tailles ─────────────────
    //
    // The engine never sees a size. It sees one footprint per piece — the union of that piece over
    // the sizes being printed — exactly as Ostara builds it (`oexportnesting.cpp` : « The engine
    // never sees a size »).
    QVector<SizeLayer> engineLayers;
    engineLayers.reserve(sizes.size());

    QVector<PrintLayer> layers;
    layers.reserve(sizes.size());

    for (int index = 0; index < sizes.size(); ++index)
    {
        const CutSize &size = sizes.at(index);

        PrintLayer layer;
        layer.label = size.label;
        layer.sizeId = index < sizeIds.size() ? sizeIds.at(index) : QString();
        // **The children belong to the RUN's first size, which is its smallest.** Ostara draws them
        // on the first layer of the range, and only there — `const bool isReference = (l == 0);`
        // (`oexportcanvas.cpp:1812-1814`), grain line included (`:1852-1857`). Naming the payload's
        // own reference instead meant a run that does not tick the smallest size of the PATTERN
        // carried no cartouche, no annotation, no miniature and no grain line at all, while Ostara
        // drew them on the smallest size of the run.
        layer.reference = (index == 0);

        SizeLayer engineLayer;
        engineLayer.label = size.label;

        for (const CutPiece &pose : size.poses)
        {
            if (pose.outline.size() < 3)
            {
                continue;
            }

            Piece piece;
            // The piece's NAME, and its group's KEY when it has one: two sizes of one piece are one
            // footprint, which is what makes the plan graded rather than two plans laid side by side
            // — and the two members of a group are one footprint too, which is what a group IS. The
            // footprint groups on the group when there is one, so the key travels with the name.
            piece.name = pose.name;
            if (!pose.groupKey.isEmpty())
            {
                // The same string in both fields: the key is what the footprint groups by, and it
                // is the payload's own (`g<groupId>`), so nothing has to be re-derived later.
                piece.groupName = pose.footprint;
                piece.groupKey = pose.footprint;
            }

            ContourNode node;
            node.type = QStringLiteral("Contour");
            node.polyline = ToPath64(pose.outline);
            if (node.polyline.size() < 3)
            {
                continue;
            }
            piece.contourNodes.append(node);
            engineLayer.pieces.append(piece);

            PrintPiece drawn;
            drawn.name = pose.name;
            drawn.footprint = pose.footprint;
            drawn.outline = pose.outline;
            drawn.ink = pose.ink;
            drawn.hasGrainLine = pose.hasGrainLine;
            drawn.grainA = pose.grainA;
            drawn.grainB = pose.grainB;
            drawn.arrows = pose.arrows;
            drawn.onFold = pose.onFold;
            drawn.foldA = pose.foldA;
            drawn.foldB = pose.foldB;
            drawn.decor = DecorFor(size.decor, pose);
            layer.pieces.append(drawn);
        }

        engineLayers.append(engineLayer);
        layers.append(layer);
    }

    // The drawing's own styles, read from the payload once — a graded plan tells its sizes apart
    // by them, and the plan has to draw what the pattern says.
    QDomDocument drawing;
    if (static_cast<bool>(drawing.setContent(document.payload())))
    {
        for (PrintLayer &layer : layers)
        {
            LayerStyle(drawing, layer.sizeId, layer.colour, layer.dashes, layer.width);
        }
    }

    plan.layers = layers;
    plan.footprint = Footprint::compute(engineLayers);
    if (plan.footprint.polygons.empty())
    {
        plan.error = QObject::tr("The size range carries no usable contour.");
        return plan;
    }

    // ── Le bac, et la grille qu'on lui donne ────────────────────────────────────────────────────
    //
    // The margin and the gap arrive in centimetres from the panel and are turned into pixels once,
    // here: the engine consumes every length on the same scale.
    const double marginPx = marginCm * kPxPerCm;
    const double gapPx = gapCm * kPxPerCm;

    std::vector<Bin::Zone> zones;
    if (!coverBlockPx.isEmpty())
    {
        zones.push_back({coverBlockPx.x(), coverBlockPx.y(), coverBlockPx.width(),
                         coverBlockPx.height()});
    }

    plan.reservedPx = coverBlockPx;

    const Bin bin(binWidthPx, marginPx, zones);

    Engine::Config config;
    config.gap = gapPx;

    // The grid, and only on a tiled run. **This is the print run's own setting and the cutting
    // plan has none**: it is what lets the engine keep a footprint's border off a cut line and
    // spend the dead band the last row keeps for free.
    if (sheetHeightMm > 0.0 && plan.columns > 0)
    {
        config.artboardCount = plan.columns;
        config.artboardHeight = PrintMetrics::MmToPx(sheetHeightMm);
    }

    bool abandoned = false;
    const Engine engine{config};
    // An empty angle list is a piece nobody constrained: the engine keeps its whole sweep. That is
    // what a print run is — the cutting plan's angles, twins and fold route do not exist here.
    plan.placements = engine.place(bin, plan.footprint.polygons, {}, {}, &abandoned);

    if (abandoned)
    {
        plan.error = QObject::tr("The laying out was given up.");
        return plan;
    }

    if (plan.placements.size() != plan.footprint.polygons.size())
    {
        plan.error = QObject::tr("The nesting engine returned an incomplete result.");
        return plan;
    }

    // The origin of every footprint, read before anything compares two plans: the arbitration
    // measures the paper a plan prints, and a plan is measured through these.
    for (const Clipper2Lib::Path64 &polygon : plan.footprint.polygons)
    {
        plan.origins.append(MinCorner(polygon));
    }

    // ── L'arbitrage : deux moteurs, le moins de feuilles gagne ─────────────────────────────────
    //
    // Sur un run TUILÉ, un second moteur entre en jeu. Le moteur de bande minimise une HAUTEUR : la
    // page n'existe pas pour lui. Le moteur tuilé paie en PAGES. Les deux plans sont calculés et
    // c'est celui qui imprime le MOINS de feuilles qui est gardé — le plan historique reste la
    // référence, et un autre ne le remplace que s'il fait strictement mieux.
    if (config.artboardHeight > 0.0 && config.artboardCount > 0)
    {
        const double sheetWidthPx =
            binWidthPx / static_cast<double>(std::max(plan.columns, 1));

        int bestSheets = PrintedSheets(plan.placements, plan.footprint.polygons, plan.origins,
                                       bin, binWidthPx, sheetHeightMm, plan.columns,
                                       coverBlockPx);

        TiledEngine::Spec spec;
        spec.sheetWidth = sheetWidthPx;
        spec.sheetHeight = config.artboardHeight;
        spec.gap = gapPx;
        spec.allowRotation = true;

        const TiledEngine::Spec::Order orders[] = {TiledEngine::Spec::Order::HolderArea,
                                                   TiledEngine::Spec::Order::HolderHeight,
                                                   TiledEngine::Spec::Order::HolderWidth};

        for (const TiledEngine::Spec::Order order : orders)
        {
            spec.order = order;

            bool tiledAbandoned = false;
            const std::vector<Placement> tiled =
                TiledEngine(spec).place(bin, plan.footprint.polygons, &tiledAbandoned);

            if (tiledAbandoned)
            {
                plan.error = QObject::tr("The laying out was given up.");
                return plan;
            }

            if (tiled.size() != plan.footprint.polygons.size())
            {
                continue; // that attempt placed nothing usable: it does not compete
            }

            bool allPlaced = true;
            for (const Placement &placement : tiled)
            {
                if (placement.rotation < 0.0)
                {
                    allPlaced = false;
                    break;
                }
            }
            if (!allPlaced)
            {
                continue;
            }

            const int tiledSheets = PrintedSheets(tiled, plan.footprint.polygons, plan.origins,
                                                  bin, binWidthPx, sheetHeightMm, plan.columns,
                                                  coverBlockPx);

            if (tiledSheets < bestSheets)
            {
                bestSheets = tiledSheets;
                plan.placements = tiled;
            }
        }
    }

    // A piece the engine could not place is marked with a negative rotation — its one sentinel.
    for (std::size_t index = 0; index < plan.placements.size(); ++index)
    {
        if (plan.placements.at(index).rotation < 0.0)
        {
            const QString name = index < static_cast<std::size_t>(plan.footprint.names.size())
                                     ? plan.footprint.names.at(static_cast<int>(index))
                                     : QString();
            plan.error = QObject::tr("The piece \"%1\" does not fit in this width of fabric.")
                             .arg(name);
            return plan;
        }
    }


    // Each drawn piece is placed at its footprint — by name, which is the key `Footprint::compute`
    // gives a piece that belongs to no group.
    QHash<QString, int> footprintIndex;
    for (int index = 0; index < static_cast<int>(plan.footprint.keys.size()); ++index)
    {
        footprintIndex.insert(plan.footprint.keys.at(index), index);
    }

    for (PrintLayer &layer : plan.layers)
    {
        for (PrintPiece &piece : layer.pieces)
        {
            piece.placementIndex = footprintIndex.value(piece.footprint, -1);
        }
    }

    plan.usedHeightPx = Engine::UsedHeight(plan.placements, plan.footprint.polygons, bin);

    // ── Le tuilage, lu sur le résultat ──────────────────────────────────────────────────────────
    plan.grid = ExportArtboards::Compute(plan.binWidthPx, plan.usedHeightPx, sheetHeightMm, plan.columns);
    plan.RefreshSheets();

    plan.ok = true;
    return plan;
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPlan::Pages(bool coverHasItsOwnSheet) const -> int
{
    const int printed = grid.valid ? sheets.count : 1;
    return printed + (coverHasItsOwnSheet ? 1 : 0);
}

//---------------------------------------------------------------------------------------------------------------------
void PrintPlan::RefreshSheets()
{
    if (!grid.valid)
    {
        return;
    }

    // The placed footprint of every piece — the envelope the engine placed, read from where the
    // placement now stands. The ink is the shape, and never the box around it: a piece turned
    // forty-five degrees has a box far larger than itself.
    QVector<QPolygonF> ink;
    ink.reserve(static_cast<int>(footprint.polygons.size()));

    for (int index = 0; index < static_cast<int>(footprint.polygons.size()); ++index)
    {
        const Clipper2Lib::Path64 &polygon = footprint.polygons.at(static_cast<std::size_t>(index));
        const Placement &placement = placements.at(static_cast<std::size_t>(index));
        const QPointF origin = origins.at(index);

        const double radians = placement.rotation * 3.14159265358979323846 / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);

        QPolygonF shape;
        shape.reserve(static_cast<int>(polygon.size()));
        for (const auto &point : polygon)
        {
            const double x = static_cast<double>(point.x) / kScale - origin.x();
            const double y = static_cast<double>(point.y) / kScale - origin.y();
            shape.append(QPointF(x * c - y * s + placement.x, x * s + y * c + placement.y));
        }
        ink.append(shape);
    }

    sheets = ExportSheets::Compute(grid, ink, reservedPx);
}
