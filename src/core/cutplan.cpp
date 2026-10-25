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
#include <QDebug>

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

//---------------------------------------------------------------------------------------------------------------------
/// Whether a rectangle whose fold is on @p foldOnTheLeft can carry @p pose.
///
/// One rule, written once: a pose cut on the fold only goes where its own fold side is the
/// rectangle's. The side comes from `Orient`, which took it from `CuttingRules::FoldAngle` — the same
/// call Ostara makes, on the same geometry.
auto Suits(const CutPiece &pose, bool foldOnTheLeft, const QHash<QString, bool> &foldRightByKey)
    -> bool
{
    return !pose.onFold || foldRightByKey.value(pose.key, false) != foldOnTheLeft;
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
         const QHash<QString, bool> &foldRightByKey, bool foldedFabric, double binWidthPx,
         double marginPx, double gapPx) -> Layout
{
    Layout layout;

    // ── Quelles formes ce rectangle peut porter ───────────────────────────────────────────────
    //
    // ⚠️ **La forme qui va du mauvais côté n'entre pas, même pour rien.** Ce rectangle-ci a son pli
    // à gauche : une pièce dont le pli tombe à droite n'y est pas posée du tout, c'est son miroir qui
    // l'est. La garder ferait deux empreintes pour une pièce — elle serait coupée deux fois — et la
    // poser de travers mettrait son trait de pli au milieu du tissu.
    QVector<CutPiece> laid;
    laid.reserve(poses.size());
    for (const CutPiece &pose : poses)
    {
        if (foldedFabric && !Suits(pose, true, foldRightByKey))
        {
            continue;
        }
        laid.append(pose);
    }

    if (laid.isEmpty())
    {
        layout.error = QObject::tr("This pattern carries no piece that can be laid on the fold.");
        return layout;
    }

    layout.footprint = FootprintsOf(laid);
    if (layout.footprint.polygons.empty())
    {
        layout.error = QObject::tr("This pattern carries no usable contour.");
        return layout;
    }

    QHash<QString, CutPiece> poseByKey;
    for (const CutPiece &pose : laid)
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
        // The wall belongs to the RECTANGLE, not to the piece: this one folds on its left, so a piece
        // cut on the fold is glued to the left wall — the one place where the margin is zero and the
        // cloth actually folds. Ostara's `ConstraintsFor` writes the same thing.
        anchors.push_back(foldedFabric && pose.onFold ? Anchor::LeftWall : Anchor::Free);
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
              const QHash<QString, bool> &foldRightByKey, double binWidthPx, double marginPx,
              double gapPx) -> Layout
{
    Layout layout;

    // ── Les deux formes de chaque pose ────────────────────────────────────────────────────────
    //
    // Filed by base key, and by POSES and not by names: which of the two goes on which edge is a
    // property of the piece (`Suits`), never of the fact that one of them is the stored form.
    QHash<QString, QVector<CutPiece>> forms;
    QStringList order;
    for (const CutPiece &pose : poses)
    {
        if (!pose.onFold)
        {
            continue;
        }

        const QString base = BaseKey(pose.key);
        if (!forms.contains(base))
        {
            order.append(base);
        }
        forms[base].append(pose);
    }

    /// The form of @p base that a rectangle folding on @p foldOnTheLeft can carry, or nothing.
    const auto formFor = [&foldRightByKey](const QVector<CutPiece> &candidates,
                                           bool foldOnTheLeft) -> const CutPiece * {
        for (const CutPiece &candidate : candidates)
        {
            if (Suits(candidate, foldOnTheLeft, foldRightByKey))
            {
                return &candidate;
            }
        }
        return nullptr;
    };

    // A band needs both forms: one for each edge. Without them there is no second edge to open.
    QStringList paired;
    for (const QString &base : order)
    {
        if (formFor(forms.value(base), true) != nullptr
            && formFor(forms.value(base), false) != nullptr)
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
        return widthOf(*formFor(forms.value(a), true)) > widthOf(*formFor(forms.value(b), true));
    });

    const QString leftBase = paired.at(0);
    const QString rightBase = paired.at(1);

    const double leftPieceWidth = widthOf(*formFor(forms.value(leftBase), true));
    const double rightPieceWidth = widthOf(*formFor(forms.value(rightBase), false));

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

    // 0 : the left band, fold on its LEFT edge.  1 : the right band, fold on its RIGHT edge.
    // 2 : the column under them, fold on its left — the same rectangle the one-edge plan is.
    const bool seatFoldsOnTheLeft[3] = {true, false, true};

    // ── The forms this plan will lay ───────────────────────────────────────────────────────────
    //
    // Exactly one form per piece: the one its own fold side allows in the rectangle it lands in.
    // Everything that cannot go there is left out — laying both forms would cut the piece twice, and
    // laying the wrong one puts its fold line in the middle of the cloth with its selvedge on the
    // fold, which looks like a plan.
    QVector<CutPiece> chosen;
    QHash<QString, int> seatOfKey;
    for (const CutPiece &pose : poses)
    {
        if (!pose.onFold)
        {
            chosen.append(pose);
            seatOfKey.insert(pose.key, 2);
            continue;
        }

        const QString base = BaseKey(pose.key);
        const int seat = base == leftBase ? 0 : (base == rightBase ? 1 : 2);
        if (!Suits(pose, seatFoldsOnTheLeft[seat], foldRightByKey))
        {
            continue;
        }
        chosen.append(pose);
        seatOfKey.insert(pose.key, seat);
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
    for (const QString &key : layout.footprint.keys)
    {
        const CutPiece pose = poseByKey.value(key);
        layout.poses.append(pose);
        angles.push_back(anglesByKey.value(key));
    }

    for (std::size_t index = 0; index < layout.footprint.polygons.size(); ++index)
    {
        layout.origins.append(MinCorner(layout.footprint.polygons.at(index)));
    }

    // ── The three rectangles ──────────────────────────────────────────────────────────────────
    //
    // The fold side of each one is `seatFoldsOnTheLeft`, written above and nowhere else: it decides
    // both the margins and the wall a fold piece is glued to, and two statements of it would drift
    // apart in silence.
    struct Seat
    {
        double width;
        Bin::Margins margins;
    };
    const Seat seats[3] = {{leftWidth, CuttingRules::SeatMargins(seatFoldsOnTheLeft[0], marginPx)},
                           {rightWidth, CuttingRules::SeatMargins(seatFoldsOnTheLeft[1], marginPx)},
                           {binWidthPx, CuttingRules::SeatMargins(seatFoldsOnTheLeft[2], marginPx)}};

    // A rectangle is paid for before it receives anything: the test is on the USABLE width.
    const auto usable = [&seats](int seat) {
        return seats[seat].width - seats[seat].margins.left - seats[seat].margins.right;
    };

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
                                            ? (seatFoldsOnTheLeft[seat] ? Anchor::LeftWall
                                                                        : Anchor::RightWall)
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
    // a rectangle whose fold side is its own — read off the form, never off its name — and a
    // rectangle too narrow to receive it is not tried at all.
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

                // A form goes to its own side, and nowhere else — the side read off the form itself.
                if (!Suits(layout.poses.at(static_cast<int>(index)), seatFoldsOnTheLeft[seat],
                           foldRightByKey))
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
/// The direction of every pose, and **the side its fold demands**, read once — the same reading the
/// two candidates share, so neither can decide a side differently.
///
/// ⚠️ **The side is READ OFF THE PIECE, never assumed from which form it is.** A pose and its mirror
/// are the same garment twice: one of them wants the fold on the left of the cloth, the other on the
/// right, and which is which depends on the piece's own geometry — not on the fact that one is called
/// the natural and the other the twin. Ostara's cutting plan does exactly this
/// (`VariantFor`, `oexportnesting.cpp`), and a convention written in its place lays half the pieces
/// against the selvedge with the fold line floating in the middle of the cloth: a plan that looks
/// finished and is wrong.
auto Orient(const QVector<CutPiece> &poses, bool foldedFabric, bool oneWay,
            QHash<QString, std::vector<double>> *anglesByKey,
            QHash<QString, bool> *foldRightByKey, QString *error) -> bool
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
            foldRightByKey->insert(pose.key, foldOnTheRight);
            // Diagnostic du banc : le côté du pli tel qu'il est LU sur la pièce, et la rotation qui
            // en découle. Une pièce dont le pli est à droite et qui sort ici `foldOnTheRight=0` est
            // lue à l'envers, et aucune ancre ne peut le rattraper.
            continue;
        }

        CuttingRules::Direction direction;
        direction.known = pose.grainKnown;
        direction.angle = pose.grain;
        direction.arrows = static_cast<CuttingRules::Arrows>(pose.arrows);

        anglesByKey->insert(pose.key, CuttingRules::AllowedAngles(direction, oneWay));
        foldRightByKey->insert(pose.key, false);
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
    QHash<QString, bool> foldRightByKey;

    if (!Orient(size.poses, folded, request.oneWay, &anglesByKey, &foldRightByKey, &plan.error))
    {
        return plan;
    }

    // ── Le candidat à un bord ───────────────────────────────────────────────────────────────────
    //
    // The column, fold on its LEFT edge. It receives every pose: which FORM of a fold piece goes in
    // it is `Lay`'s business, and it is read off the piece's own fold side — a piece whose fold
    // falls on the right is laid as its mirror, because the rectangle's fold is on the left and the
    // cloth cannot be folded on both sides at once.
    if (size.poses.isEmpty())
    {
        plan.error = QObject::tr("No piece can be laid out for this size.");
        return plan;
    }

    const Layout one =
        Lay(size.poses, anglesByKey, foldRightByKey, folded, plan.binWidthPx, marginPx, gapPx);
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
        const Layout two = TwoEdges(size.poses, anglesByKey, foldRightByKey, plan.binWidthPx,
                                    marginPx, gapPx);

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
