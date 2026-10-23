/************************************************************************
 **
 **  @file   pattern.h
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

#ifndef PATTERN_H
#define PATTERN_H

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QHash>
#include <QStringList>
#include <QVector>

struct ImportedDocument;

/// The drawing of one piece's own children — its cartouche, its annotations, the grain line and the
/// fold mark as they were DRAWN and not as a pair of points.
///
/// **Ostara writes them in the PAYLOAD now, once per size.** The payload's `<piece>` carries its own
/// `<decor>`, written in the piece's own frame — the same frame its outline travels in — and the same
/// ink is copied into every size. That is the whole contract: a size asked for finds its children
/// already there, and Rabnest places them; it never resolves a position, never shifts one.
struct PieceDecor
{
    /// The `<decor>` children, in the piece's own frame.
    QString content;

    /// A box around that content, in the pattern's pixels. Crude on purpose — it is only what an SVG
    /// needs to know so as not to clip, and a control point included in it is a pixel of margin.
    QRectF box;
};

/// One pose of one piece — a piece laid once, or once per cut.
///
/// A pose is what a cut plan places. It is not a piece: a piece cut twice is two poses of the same
/// piece, and a piece cut on the fold, on folded cloth, is one pose of its half plus the twin that
/// lets it sit on either edge of the fold.
struct CutPiece
{
    QString key;   ///< identity of THIS pose — what the nesting groups and what a result is read by
    QString name;  ///< the piece, as the user calls it; repeated across poses

    /// The outline to lay, in the pattern's own pixels. It is the **half** on folded cloth and the
    /// whole form on flat cloth — chosen when the payload is read, because the choice is the
    /// cloth's and not the piece's.
    QVector<QPointF> outline;

    bool onFold = false;
    QPointF foldA;
    QPointF foldB;

    /// The grain line, in the piece's own frame — the segment and not the angle. A plan reads the
    /// angle; it DRAWS the line, and a line without its ends is a direction with nowhere to go.
    bool hasGrainLine = false;
    QPointF grainA;
    QPointF grainB;

    /// The direction, in the pattern's frame — **not** relative to the drawing, which is already
    /// turned by a placement. `known == false` is a piece a cutting plan must refuse to lay.
    bool grainKnown = false;
    double grain = 0.0;
    int arrows = 0;

    int cutFabric = 0;
    int cutLining = 0;
};

/// One size, already expanded: every pose the plan will place.
struct CutSize
{
    QString label;
    QVector<CutPiece> poses;

    /// This size's decor, by piece name.
    ///
    /// ⚠️ **Per size, and it could not be otherwise once the payload projects it.** Ostara writes the
    /// drawing once and seats it on every size; the seat differs a little from one size to the next,
    /// so two sizes no longer carry the same bytes. A table shared by every size kept only the last
    /// one read and drew it everywhere — invisible while the copies were identical, and wrong the
    /// moment they stopped being.
    QHash<QString, PieceDecor> decor;
};

/**
 * @brief Reads the pieces of an Ostara PDF, as a plan needs them.
 *
 * **The payload is the only source, and it says everything.** Ostara writes, per piece, its outline
 * in the pattern's frame, its direction with the arrow type, the axis of its fold, the form the
 * drawing does not carry, and the quantity it is cut in — `metadata/pieces`. None of it can be read
 * back from the drawing: the drawing is placed, its grain is a line rather than an angle, its fold
 * is a pair of arrows rather than an axis, and its cut quantity is nowhere at all.
 *
 * What is built here is the list of POSES: the quantity expanded, the twin of a fold piece added
 * when the cloth is folded, and the outline of each pose chosen to match the cloth.
 */
class Pattern
{
public:
    /**
     * @param sizeIds      the layers the user kept, by their SVG id
     * @param pieceNames   the pieces the user kept, by name
     * @param foldedFabric the cloth the plan will be laid on — it alone decides which form of a
     *                     fold piece is laid, and whether the twin exists
     * @return false when the payload carries no pieces block, or when a kept size has none; @p error
     *         then says which
     */
    /// @param decor  what each piece is DRAWN with, by piece name — filled from the decor layer, and
    ///               left empty when the file carries none
    static bool Load(const ImportedDocument &document, const QStringList &sizeIds,
                     const QStringList &pieceNames, bool foldedFabric, QVector<CutSize> &out,
                     QString *error);
};

#endif  // PATTERN_H
