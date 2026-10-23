/************************************************************************
 **
 **  @file   cutplanrequest.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  What a cutting plan is asked for — and nothing else.
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

#ifndef CUTPLANREQUEST_H
#define CUTPLANREQUEST_H

#include <QtGlobal>

/// The fabric a cutting plan is laid on — the whole of what the user decides.
///
/// **Five numbers and two flags, and there is nothing else.** Everything a plan needs to know about
/// a *piece* — its direction, its fold, how many times it is cut — belongs to the pattern and
/// travels in the PDF. What belongs here belongs to the cloth the user is about to buy, and to
/// nothing else: it is her decision, taken once, and the same pattern gives a different plan for a
/// different laize.
struct CutPlanRequest
{
    /// The laize, as the fabric is sold — and as the field reads on Ostara's own cutting tab.
    qreal widthCm = 140.0;

    /// The space kept from the edges of the cloth, and the space between two pieces.
    qreal marginCm = 0.5;
    qreal gapCm = 0.5;

    /// The fabric is folded in two, so the plan is laid on **half** the laize and a piece cut on the
    /// fold is laid as its half, glued to the fold.
    bool foldedFabric = false;

    /// « The same way round » — no piece is turned a half-turn. It removes a SENSE, not an axis.
    bool oneWay = false;

    /// The bin the nesting is given: the laize, halved when the cloth is folded.
    ///
    /// The figure typed in does not move — 140 entered here is a plan laid on 70 — which is what
    /// Ostara's own field says about itself, and the reason the halving is done here once rather
    /// than in every reader of the width.
    qreal BinWidthCm() const { return foldedFabric ? widthCm / 2.0 : widthCm; }
};

#endif  // CUTPLANREQUEST_H
