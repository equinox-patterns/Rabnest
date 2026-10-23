/************************************************************************
 **
 **  @file   cutplan.h
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

#ifndef CUTPLAN_H
#define CUTPLAN_H

#include "core/cutplanrequest.h"
#include "core/pattern.h"

#include <QHash>

#include "model/Placement.h"    // Trame
#include "nesting/Footprint.h"  // Trame

#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <vector>

/// One rectangle of folded cloth — where it is in the strip, and which side holds the fold.
///
/// The four seams a folded plan draws: the column, or the two bands and the column under them. A
/// one-edge plan has one of these and the fold sits on its left; it is drawn so that the user can
/// see how to fold her cloth before cutting.
struct FoldZone
{
    QRectF rect;                    ///< in the plan's own pixels
    bool foldOnTheLeft = true;      ///< the side that is a fold; the other is a selvedge
};

/// @brief A footprint where the plan put it.
///
/// `(x, y) + R(rotation) · (p − origin)` — the placement's reference point is the min corner of the
/// polygon BEFORE rotation, so that corner is taken back out and the rotation is measured from it.
///
/// **One function, because a plan has one geometry.** The canvas draws it and the checker measures
/// it; a second copy of this arithmetic would let the screen and the measurement disagree about a
/// plan they both call correct, and nothing would say which one is lying.
auto PlacedOutline(const Clipper2Lib::Path64 &polygon, const Placement &placement,
                   const QPointF &origin) -> QPolygonF;

/// @brief One point where the plan put it — the same transform, for what is drawn with the piece.
///
/// An annotation belongs to the piece: a grain line or a fold that stayed behind when its piece
/// moved would be worse than none at all, because it looks like information.
auto PlacedPoint(const QPointF &point, const Placement &placement, const QPointF &origin)
    -> QPointF;

/// The cutting plan: where each pose goes, and how much cloth that costs.
///
/// **Nothing is written.** This is the plan, not a file: the canvas draws it and the status line
/// reads its height. A plan that leaves Rabnest as a file is a different feature, and a different
/// promise about what the thing is.
struct CutPlan
{
    QString sizeLabel;

    /// The cloth the plan was asked for, as she typed it — the laize is the unfolded one, which is
    /// what she buys, and it is what names the run in the list. Kept here so a run can say what it
    /// is without the request that made it.
    double fabricWidthCm = 0.0;
    bool oneWay = false;

    /// What the run asked for beyond the cloth: the margin it must leave at the edge of the fabric,
    /// and the gap it must leave between two pieces. Carried because the OUTPUTS DOCK has to answer
    /// « what did I ask for that one? » months later, and a figure that lives only in a dialog she
    /// has already closed cannot answer it. Absent from the earlier plans, hence the defaults.
    double marginCm = 0.0;
    double gapCm = 0.0;

    double binWidthPx = 0.0;   ///< the laize, halved when the cloth is folded
    double usedHeightPx = 0.0; ///< the length of fabric the plan costs, bottom margin included
    bool foldedFabric = false;

    /// Where the engine put each footprint, in the bin's own coordinates.
    std::vector<Placement> placements;

    /// The footprints, and the poses they came from — index-aligned with `placements`.
    Footprint::Result footprint;
    QVector<CutPiece> poses;

    /// The min corner of each footprint polygon, in pixels: the origin a placement's rotation is
    /// measured from. Drawing a pose is `(x, y) + R(rotation) · (p − origin)`, and nothing else.
    QVector<QPointF> origins;

    QVector<FoldZone> zones;

    /// What each piece is drawn with, by name — its cartouche, its annotations, its grain line as it
    /// was drawn. Carried by the PLAN and not by each pose: ten poses of one piece share one drawing.
    QHash<QString, PieceDecor> decor;

    /// How long this plan took to lay out, in seconds — measured around the engine, because the
    /// figure the user reads is the time HER plan cost and not the time the batch took.
    double seconds = 0.0;

    bool ok = false;
    QString error;

    /// Lays one size out. Synchronous: a plan is seconds, and the caller decides whether that is
    /// worth a thread.
    ///
    /// **The rules are the socle's, and they are the very ones Ostara applies** — the same allowed
    /// rotations, the same fold angle, the same margins per rectangle, the same engine. Nothing is
    /// decided here that Ostara decides elsewhere.
    static CutPlan Generate(const CutSize &size, const CutPlanRequest &request,
                            const QHash<QString, PieceDecor> &decor = {});
};

#endif  // CUTPLAN_H
