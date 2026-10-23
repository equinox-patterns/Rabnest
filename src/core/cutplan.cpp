/************************************************************************
 **
 **  @file   cutplan.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The cutting plan — one size laid on one length of fabric.
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

#include "nesting/Bin.h"           // Trame
#include "nesting/CuttingRules.h"  // Trame — les règles, celles d'Ostara
#include "nesting/Engine.h"        // Trame

#include <QHash>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace
{
/// The engine's fixed point. A pixel is worth 1e6 units, as everywhere in the socle.
constexpr double kScale = 1000000.0;

constexpr double kPxPerCm = 96.0 / 2.54;

/// The suffix the mirrored twin of a fold piece carries. Not cosmetic: a footprint is the union of
/// everything sharing a key, so the two forms of one piece must never share one.
constexpr auto kTwinSuffix = "-m";

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
auto ContourOf(const QVector<QPointF> &points) -> std::vector<std::pair<double, double>>
{
    std::vector<std::pair<double, double>> contour;
    contour.reserve(static_cast<std::size_t>(points.size()));
    for (const QPointF &point : points)
    {
        contour.emplace_back(point.x(), point.y());
    }
    return contour;
}

//---------------------------------------------------------------------------------------------------------------------
auto FoldLineOf(const CutPiece &pose) -> CuttingRules::Line
{
    CuttingRules::Line line;
    line.x1 = pose.foldA.x();
    line.y1 = pose.foldA.y();
    line.x2 = pose.foldB.x();
    line.y2 = pose.foldB.y();
    return line;
}

//---------------------------------------------------------------------------------------------------------------------
/// The box of a polygon once turned — what tells how wide a band has to be.
auto RotatedBox(const Clipper2Lib::Path64 &polygon, double angle) -> QRectF
{
    const double radians = angle * 3.14159265358979323846 / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);

    double lowX = 0.0;
    double highX = 0.0;
    double lowY = 0.0;
    double highY = 0.0;
    bool first = true;
    for (const auto &point : polygon)
    {
        const double x = static_cast<double>(point.x) / kScale;
        const double y = static_cast<double>(point.y) / kScale;
        const double turnedX = x * c - y * s;
        const double turnedY = x * s + y * c;

        if (first)
        {
            lowX = highX = turnedX;
            lowY = highY = turnedY;
            first = false;
        }
        else
        {
            lowX = std::min(lowX, turnedX);
            highX = std::max(highX, turnedX);
            lowY = std::min(lowY, turnedY);
            highY = std::max(highY, turnedY);
        }
    }
    return QRectF(lowX, lowY, highX - lowX, highY - lowY);
}

//---------------------------------------------------------------------------------------------------------------------
auto BaseKey(const QString &key) -> QString
{
    return key.endsWith(QLatin1String(kTwinSuffix)) ? key.left(key.size() - 2) : key;
}

auto IsTwin(const QString &key) -> bool
{
    return key.endsWith(QLatin1String(kTwinSuffix));
}

//---------------------------------------------------------------------------------------------------------------------
/// A plan, before it is known whether it is the one kept: its own footprints and its own geometry.
struct Layout
{
    Footprint::Result footprint;
    QVector<CutPiece> poses;
    QVector<QPointF> origins;
    std::vector<Placement> placements;
    QVector<FoldZone> zones;
    double usedHeightPx = 0.0;
    bool ok = false;
    QString error;
};

//---------------------------------------------------------------------------------------------------------------------
/// Builds the footprints of @p poses, in the order the engine will see them.
auto FootprintsOf(const QVector<CutPiece> &poses) -> Footprint::Result
{
    SizeLayer layer;
    for (const CutPiece &pose : poses)
    {
        Piece piece;
        piece.name = pose.key;  // the key, never the name: two copies are two footprints

        ContourNode node;
        node.type = QStringLiteral("Contour");
        node.polyline = ToPath64(pose.outline);
        if (node.polyline.size() < 3)
        {
            continue;
        }

        piece.contourNodes.append(node);
        layer.pieces.append(piece);
    }

    QVector<SizeLayer> layers;
    layers.append(layer);
    return Footprint::compute(layers);
}

//---------------------------------------------------------------------------------------------------------------------
/// One strip of fabric, one column of pieces — the plan every flat run makes, and the one-edge
/// folded run too.
auto Lay(const QVector<CutPiece> &poses, const QHash<QString, std::vector<double>> &anglesByKey,
         const QHash<QString, Anchor> &anchorByKey, bool foldedFabric, double binWidthPx,
         double marginPx, double gapPx) -> Layout
{
    Layout layout;

    layout.footprint = FootprintsOf(poses);
    if (layout.footprint.polygons.empty())
    {
        layout.error = QObject::tr("This pattern carries no usable contour.");
        return layout;
    }

    QHash<QString, CutPiece> poseByKey;
    for (const CutPiece &pose : poses)
    {
        poseByKey.insert(pose.key, pose);
    }

    std::vector<std::vector<double>> angles;
    std::vector<Anchor> anchors;
    angles.reserve(layout.footprint.keys.size());
    anchors.reserve(layout.footprint.keys.size());

    for (const QString &key : layout.footprint.keys)
    {
        const CutPiece pose = poseByKey.value(key);
        layout.poses.append(pose);
        angles.push_back(anglesByKey.value(key));
        anchors.push_back(anchorByKey.value(key, Anchor::Free));
    }

    for (std::size_t index = 0; index < layout.footprint.polygons.size(); ++index)
    {
        layout.origins.append(MinCorner(layout.footprint.polygons.at(index)));
    }

    // A flat run keeps the same margin on the four edges. A folded one keeps NOTHING on the fold:
    // a piece cut on the fold has its fold line ON the fold, and a margin there would push it off.
    const Bin bin = foldedFabric
                        ? Bin(binWidthPx, CuttingRules::SeatMargins(true, marginPx),
                              std::vector<Bin::Zone>{})
                        : Bin(binWidthPx, marginPx);

    Engine::Config config;
    config.gap = gapPx;

    bool abandoned = false;
    const Engine engine{config};
    layout.placements = engine.place(bin, layout.footprint.polygons, angles, anchors, &abandoned);

    if (abandoned)
    {
        layout.error = QObject::tr("The laying out was given up.");
        return layout;
    }

    if (layout.placements.size() != layout.footprint.polygons.size())
    {
        layout.error = QObject::tr("The nesting engine returned an incomplete result.");
        return layout;
    }

    // A piece the engine could not place is marked with a negative rotation — its one sentinel.
    for (std::size_t index = 0; index < layout.placements.size(); ++index)
    {
        if (layout.placements.at(index).rotation < 0.0)
        {
            layout.error = QObject::tr("The piece \"%1\" does not fit in this width of fabric.")
                               .arg(layout.poses.at(static_cast<int>(index)).name);
            return layout;
        }
    }

    layout.usedHeightPx = Engine::UsedHeight(layout.placements, layout.footprint.polygons, bin);

    if (foldedFabric)
    {
        FoldZone zone;
        zone.rect = QRectF(0.0, 0.0, binWidthPx, layout.usedHeightPx);
        zone.foldOnTheLeft = true;
        layout.zones.append(zone);
    }

    layout.ok = true;
    return layout;
}

//---------------------------------------------------------------------------------------------------------------------
/// **The second way of folding** — two bands whose folds are the two long edges of the stack, and the
/// column under them.
///
/// The split of the bands is a rule and not a search: each takes the width of its own fold piece,
/// and the surplus is shared equally. Each band pays its selvedge and **nothing on its fold**.
///
/// ⚠️ **A form goes to the edge it asks for, and the other form leaves the plan.** The two forms of
/// one piece are one piece: laying both would cut it twice, and laying the wrong one puts its fold
/// line in the middle of the cloth with its selvedge on the fold — which looks like a plan. So the
/// widest pair gives its natural form to the left band, the next pair its mirror to the right one,
/// and every other pair its natural form to the column, whose fold is on the left like the left
/// band's. A form only ever enters a rectangle whose fold side is its own.
auto TwoEdges(const QVector<CutPiece> &poses, const QHash<QString, std::vector<double>> &anglesByKey,
              const QHash<QString, Anchor> &anchorByKey, double binWidthPx, double marginPx,
              double gapPx) -> Layout
{
    Layout layout;

    // ── The pairs ──────────────────────────────────────────────────────────────────────────────
    QHash<QString, CutPiece> naturals;
    QHash<QString, CutPiece> twins;
    QStringList order;

    for (const CutPiece &pose : poses)
    {
        if (!pose.onFold)
        {
            continue;
        }

        const QString base = BaseKey(pose.key);
        if (IsTwin(pose.key))
        {
            twins.insert(base, pose);
        }
        else
        {
            naturals.insert(base, pose);
            order.append(base);
        }
    }

    // A band needs both forms: one for each edge. Without them there is no second edge to open.
    QStringList paired;
    for (const QString &base : order)
    {
        if (twins.contains(base))
        {
            paired.append(base);
        }
    }
    if (paired.size() < 2)
    {
        return layout;
    }

    const auto widthOf = [&anglesByKey](const CutPiece &pose) {
        const std::vector<double> angles = anglesByKey.value(pose.key);
        const double angle = angles.empty() ? 0.0 : angles.front();
        return RotatedBox(ToPath64(pose.outline), angle).width();
    };

    std::sort(paired.begin(), paired.end(), [&](const QString &a, const QString &b) {
        return widthOf(naturals.value(a)) > widthOf(naturals.value(b));
    });

    const QString leftBase = paired.at(0);
    const QString rightBase = paired.at(1);

    const double leftPieceWidth = widthOf(naturals.value(leftBase));
    const double rightPieceWidth = widthOf(twins.value(rightBase));

    // Each band pays its selvedge, so the two of them cost `2 m` before a piece is laid at all.
    const double surplus = binWidthPx - (leftPieceWidth + rightPieceWidth + 2.0 * marginPx);
    // A hundredth of a pixel of indulgence: a sum that is right to the rounding must not close the
    // door on the second edge.
    if (surplus < -0.01)
    {
        return layout;
    }

    const double leftWidth = leftPieceWidth + marginPx + surplus / 2.0;
    const double rightWidth = binWidthPx - leftWidth;

    // ── The forms this plan will lay ───────────────────────────────────────────────────────────
    //
    // Exactly one form per piece: the natural of the left band, the mirror of the right one, and the
    // natural of everything else — because the column's fold is on its left, like the left band's.
    QVector<CutPiece> chosen;
    QHash<QString, int> seatOfKey;  // 0 left band, 1 right band, 2 column

    for (const CutPiece &pose : poses)
    {
        if (!pose.onFold)
        {
            chosen.append(pose);
            seatOfKey.insert(pose.key, 2);
            continue;
        }

        const QString base = BaseKey(pose.key);
        if (base == leftBase && !IsTwin(pose.key))
        {
            chosen.append(pose);
            seatOfKey.insert(pose.key, 0);
        }
        else if (base == rightBase && IsTwin(pose.key))
        {
            chosen.append(pose);
            seatOfKey.insert(pose.key, 1);
        }
        else if (base != leftBase && base != rightBase && !IsTwin(pose.key))
        {
            chosen.append(pose);
            seatOfKey.insert(pose.key, 2);
        }
    }

    if (chosen.isEmpty())
    {
        return layout;
    }

    layout.footprint = FootprintsOf(chosen);
    if (layout.footprint.polygons.empty())
    {
        return layout;
    }

    QHash<QString, CutPiece> poseByKey;
    for (const CutPiece &pose : chosen)
    {
        poseByKey.insert(pose.key, pose);
    }

    std::vector<std::vector<double>> angles;
    std::vector<Anchor> anchors;
    for (const QString &key : layout.footprint.keys)
    {
        const CutPiece pose = poseByKey.value(key);
        layout.poses.append(pose);
        angles.push_back(anglesByKey.value(key));
        anchors.push_back(anchorByKey.value(key, Anchor::Free));
    }

    for (std::size_t index = 0; index < layout.footprint.polygons.size(); ++index)
    {
        layout.origins.append(MinCorner(layout.footprint.polygons.at(index)));
    }

    // ── The three rectangles ──────────────────────────────────────────────────────────────────
    //
    // 0 : the left band, fold on its LEFT edge.  1 : the right band, fold on its RIGHT edge.
    // 2 : the column under them, fold on its left — the same rectangle the one-edge plan is.
    struct Seat
    {
        double width;
        Bin::Margins margins;
        bool foldOnTheRight;
    };
    const Seat seats[3] = {{leftWidth, CuttingRules::SeatMargins(true, marginPx), false},
                           {rightWidth, CuttingRules::SeatMargins(false, marginPx), true},
                           {binWidthPx, CuttingRules::SeatMargins(true, marginPx), false}};

    // A rectangle is paid for before it receives anything: the test is on the USABLE width.
    const auto usable = [&seats](int seat) {
        return seats[seat].width - seats[seat].margins.left - seats[seat].margins.right;
    };
    const auto foldSeat = [&seats](int seat) { return seats[seat].foldOnTheRight; };

    std::vector<int> assignment(layout.poses.size(), 2);
    for (std::size_t index = 0; index < layout.poses.size(); ++index)
    {
        assignment.at(index) = seatOfKey.value(layout.poses.at(static_cast<int>(index)).key, 2);
    }

    // Lays an assignment out and says what it costs. False when a piece does not fit — a trial that
    // cannot be laid is not a plan, and the caller keeps the one it had.
    const auto evaluate = [&](const std::vector<int> &trial, double *height, double *upperOut,
                              double *columnOut, std::vector<Placement> *out) -> bool {
        const std::size_t count = layout.footprint.polygons.size();

        double upper = 0.0;
        double column = 0.0;
        bool any = false;

        std::vector<Placement> placed(count);

        for (int seat = 0; seat < 3; ++seat)
        {
            std::vector<Clipper2Lib::Path64> subset;
            std::vector<std::vector<double>> subsetAngles;
            std::vector<Anchor> subsetAnchors;
            std::vector<std::size_t> indices;

            for (std::size_t index = 0; index < count; ++index)
            {
                if (trial.at(index) != seat)
                {
                    continue;
                }
                indices.push_back(index);
                subset.push_back(layout.footprint.polygons.at(index));
                subsetAngles.push_back(angles.at(index));
                subsetAnchors.push_back(layout.poses.at(static_cast<int>(index)).onFold
                                            ? (foldSeat(seat) ? Anchor::RightWall : Anchor::LeftWall)
                                            : Anchor::Free);
            }

            if (subset.empty())
            {
                continue;
            }

            const Bin bin(seats[seat].width, seats[seat].margins, std::vector<Bin::Zone>{});

            Engine::Config config;
            config.gap = gapPx;

            bool abandoned = false;
            const Engine engine{config};
            std::vector<Placement> locals =
                engine.place(bin, subset, subsetAngles, subsetAnchors, &abandoned);

            if (abandoned || locals.size() != subset.size())
            {
                return false;
            }
            for (const Placement &placement : locals)
            {
                if (placement.rotation < 0.0)
                {
                    return false;
                }
            }

            const double seatHeight = Engine::UsedHeight(locals, subset, bin);
            if (seat == 2)
            {
                column = seatHeight;
            }
            else
            {
                upper = std::max(upper, seatHeight);
            }
            any = true;

            for (std::size_t index = 0; index < locals.size(); ++index)
            {
                Placement placement = locals.at(index);
                // ⚠️ **Each rectangle is nested in ITS OWN frame**, and the right band's starts at
                // zero like the left one's. Without this its pieces land on the left band — the two
                // rectangles are side by side in the plan and only one of them says so.
                if (seat == 1)
                {
                    placement.x += leftWidth;
                }
                placed.at(indices.at(index)) = placement;
            }
        }

        if (!any)
        {
            return false;
        }

        // The column stands under the bands, so everything it carries starts at their height.
        for (std::size_t index = 0; index < count; ++index)
        {
            if (trial.at(index) == 2)
            {
                placed.at(index).y += upper;
            }
        }

        *height = upper + column;
        *upperOut = upper;
        *columnOut = column;
        *out = std::move(placed);
        return true;
    };

    double best = 0.0;
    double bestUpper = 0.0;
    double bestColumn = 0.0;
    std::vector<Placement> bestPlacements;
    if (!evaluate(assignment, &best, &bestUpper, &bestColumn, &bestPlacements))
    {
        return layout;
    }

    // ── La recherche locale ─────────────────────────────────────────────────────────────────────
    //
    // **A pose moves whole, and only a strictly lower height is kept.** The two band poses never
    // move: taking them out would empty their band of the very reason it exists. A form only enters
    // a rectangle whose fold side is its own — a natural form needs a fold on the left, a mirror one
    // a fold on the right — and a rectangle too narrow to receive it is not tried at all.
    bool improved = true;
    while (improved)
    {
        improved = false;

        for (std::size_t index = 0; index < layout.poses.size() && !improved; ++index)
        {
            if (assignment.at(index) <= 1)
            {
                continue;  // the bands' own pieces do not move
            }

            for (int seat = 0; seat < 3 && !improved; ++seat)
            {
                if (seat == assignment.at(index))
                {
                    continue;
                }

                // A form goes to its own side, and nowhere else.
                if (IsTwin(layout.poses.at(static_cast<int>(index)).key) != foldSeat(seat))
                {
                    continue;
                }

                double width = 0.0;
                for (const double angle : angles.at(index))
                {
                    width = std::max(
                        width, RotatedBox(layout.footprint.polygons.at(index), angle).width());
                }
                if (width > usable(seat) + 0.01)
                {
                    continue;
                }

                std::vector<int> trial = assignment;
                trial.at(index) = seat;

                double height = 0.0;
                double upper = 0.0;
                double column = 0.0;
                std::vector<Placement> placements;
                if (!evaluate(trial, &height, &upper, &column, &placements))
                {
                    continue;
                }

                if (height < best - 1e-9)
                {
                    assignment = std::move(trial);
                    best = height;
                    bestUpper = upper;
                    bestColumn = column;
                    bestPlacements = std::move(placements);
                    improved = true;
                }
            }
        }
    }

    // The rectangles, in the column's frame — the two bands side by side, the column under them.
    FoldZone leftZone;
    leftZone.rect = QRectF(0.0, 0.0, leftWidth, bestUpper);
    leftZone.foldOnTheLeft = true;
    layout.zones.append(leftZone);

    FoldZone rightZone;
    rightZone.rect = QRectF(leftWidth, 0.0, rightWidth, bestUpper);
    rightZone.foldOnTheLeft = false;
    layout.zones.append(rightZone);

    if (bestColumn > 0.0)
    {
        FoldZone columnZone;
        columnZone.rect = QRectF(0.0, bestUpper, binWidthPx, bestColumn);
        columnZone.foldOnTheLeft = true;
        layout.zones.append(columnZone);
    }

    layout.placements = std::move(bestPlacements);
    layout.usedHeightPx = best;
    layout.ok = true;
    return layout;
}

//---------------------------------------------------------------------------------------------------------------------
/// The direction and the anchoring of every pose, read once — the same reading the two candidates
/// share, so neither can decide a side differently.
auto Orient(const QVector<CutPiece> &poses, bool foldedFabric, bool oneWay,
            QHash<QString, std::vector<double>> *anglesByKey, QHash<QString, Anchor> *anchorByKey,
            QString *error) -> bool
{
    for (const CutPiece &pose : poses)
    {
        if (foldedFabric && pose.onFold)
        {
            double angle = 0.0;
            bool foldOnTheRight = true;
            if (!CuttingRules::FoldAngle(ContourOf(pose.outline), FoldLineOf(pose), &angle,
                                         &foldOnTheRight))
            {
                *error = QObject::tr("The piece \"%1\" is cut on the fold, but its fold line is not "
                                     "one of its own edges.")
                             .arg(pose.name);
                return false;
            }

            anglesByKey->insert(pose.key, std::vector<double>{angle});
            anchorByKey->insert(pose.key,
                                foldOnTheRight ? Anchor::RightWall : Anchor::LeftWall);
            continue;
        }

        CuttingRules::Direction direction;
        direction.known = pose.grainKnown;
        direction.angle = pose.grain;
        direction.arrows = static_cast<CuttingRules::Arrows>(pose.arrows);

        anglesByKey->insert(pose.key, CuttingRules::AllowedAngles(direction, oneWay));
        anchorByKey->insert(pose.key, Anchor::Free);
    }

    return true;
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
auto PlacedPoint(const QPointF &point, const Placement &placement, const QPointF &origin)
    -> QPointF
{
    const double radians = placement.rotation * 3.14159265358979323846 / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);

    const double x = point.x() - origin.x();
    const double y = point.y() - origin.y();
    return QPointF(x * c - y * s + placement.x, x * s + y * c + placement.y);
}

//---------------------------------------------------------------------------------------------------------------------
auto PlacedOutline(const Clipper2Lib::Path64 &polygon, const Placement &placement,
                   const QPointF &origin) -> QPolygonF
{
    QPolygonF out;
    out.reserve(static_cast<int>(polygon.size()));
    for (const auto &point : polygon)
    {
        out.append(PlacedPoint(QPointF(static_cast<double>(point.x) / kScale,
                                       static_cast<double>(point.y) / kScale),
                               placement, origin));
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
auto CutPlan::Generate(const CutSize &size, const CutPlanRequest &request,
                       const QHash<QString, PieceDecor> &decor) -> CutPlan
{
    CutPlan plan;
    plan.sizeLabel = size.label;
    plan.decor = decor;
    plan.foldedFabric = request.foldedFabric;
    plan.fabricWidthCm = request.widthCm;
    plan.oneWay = request.oneWay;
    plan.marginCm = request.marginCm;
    plan.gapCm = request.gapCm;
    plan.binWidthPx = request.BinWidthCm() * kPxPerCm;

    if (plan.binWidthPx <= 0.0)
    {
        plan.error = QObject::tr("The width of the fabric is zero.");
        return plan;
    }

    const double marginPx = request.marginCm * kPxPerCm;
    const double gapPx = request.gapCm * kPxPerCm;
    const bool folded = request.foldedFabric;

    QHash<QString, std::vector<double>> anglesByKey;
    QHash<QString, Anchor> anchorByKey;

    if (!Orient(size.poses, folded, request.oneWay, &anglesByKey, &anchorByKey, &plan.error))
    {
        return plan;
    }

    // ── Le candidat à un bord ───────────────────────────────────────────────────────────────────
    //
    // The column, fold on one long side. Only the natural forms take part: the twin exists to open
    // the OTHER edge, and that is the other candidate's business.
    QVector<CutPiece> naturals;
    for (const CutPiece &pose : size.poses)
    {
        if (!folded || !IsTwin(pose.key))
        {
            naturals.append(pose);
        }
    }

    if (naturals.isEmpty())
    {
        plan.error = QObject::tr("No piece can be laid out for this size.");
        return plan;
    }

    const Layout one =
        Lay(naturals, anglesByKey, anchorByKey, folded, plan.binWidthPx, marginPx, gapPx);
    if (!one.ok)
    {
        plan.error = one.error;
        return plan;
    }

    Layout best = one;

    // ── Le candidat à deux bords ────────────────────────────────────────────────────────────────
    //
    // Two bands whose folds are the two long edges of the stack, and the column under them. It is
    // built apart because it lays a DIFFERENT set of poses: each band takes the form of the piece
    // that matches its own edge, and a form left out of a plan is not drawn and not cut.
    if (folded)
    {
        const Layout two = TwoEdges(size.poses, anglesByKey, anchorByKey, plan.binWidthPx, marginPx,
                                    gapPx);

        // Strictly shorter: on a tie the single edge stays, which is the plan that has always been.
        if (two.ok && two.usedHeightPx < best.usedHeightPx - 1e-6)
        {
            best = two;
        }
    }

    plan.footprint = best.footprint;
    plan.poses = best.poses;
    plan.origins = best.origins;
    plan.placements = best.placements;
    plan.zones = best.zones;
    plan.usedHeightPx = best.usedHeightPx;
    plan.ok = true;
    return plan;
}
