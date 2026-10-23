/************************************************************************
 **
 **  @file   printplan.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The print run: the selected sizes laid in the paper's own bin — Ostara's print
 **          nesting.
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

#ifndef PRINTPLAN_H
#define PRINTPLAN_H

#include "core/print/exportartboards.h"
#include "core/print/exportsheets.h"
#include "core/pattern.h"

#include <QColor>

#include "model/Placement.h"
#include "nesting/Footprint.h"

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

struct ImportedDocument;

/// One piece as the print run draws it — its own form, at its own size.
///
/// **The engine never sees a size**: it places one footprint per piece, and the footprint is the
/// union of that piece over the sizes being printed (Trame's `Footprint::compute`). Each size then
/// draws its own contour at the placement the envelope took, which is what a graded plan is.
struct PrintPiece
{
    QString name;

    /// The footprint this piece is placed by — the payload's key: the group's (`g<groupId>`) for a
    /// member, the piece's own name otherwise. Two members of one group stand at ONE placement.
    QString footprint;

    /// The outline to draw, in the pattern's own pixels — the WHOLE form: a print run lays the
    /// piece as it was drawn, folded cloth or not.
    QVector<QPointF> outline;

    /// What is actually DRAWN: the contour with its notches, one polygon per subpath, in the same
    /// frame as `outline`. A print is not a cutting plan — the piece it shows is the piece Ostara's
    /// canvas shows, passmarks included, and the plain outline alone would be a piece with no
    /// assembly marks on it.
    QVector<QPolygonF> ink;

    bool hasGrainLine = false;
    QPointF grainA;
    QPointF grainB;
    int arrows = 0;

    bool onFold = false;
    QPointF foldA;
    QPointF foldB;

    /// What the piece is drawn with — its cartouche, its annotations, its grain line as drawn.
    PieceDecor decor;

    /// The footprint this piece is drawn at — an index into PrintPlan::placements.
    int placementIndex = -1;
};

/// One size of the run, and the pieces it draws.
struct PrintLayer
{
    QString label;
    QString sizeId;
    QVector<PrintPiece> pieces;

    /// The size whose decor is drawn — the smallest of the range, which is the first size the
    /// payload writes. Every size draws its own ink; only this one carries a cartouche, an
    /// annotation or a grain line, exactly as Ostara's canvas does.
    bool reference{false};

    /// How this size is DRAWN — its line colour and its dash pattern, read from the payload's own
    /// layer for it.
    ///
    /// A graded plan puts several sizes on one sheet, and Ostara tells them apart by the style the
    /// pattern gives each size; the payload's drawing carries it (`<g id="Taille_46">`, whose paths
    /// hold `stroke` and `stroke-dasharray`). Read, never computed: the creator's palette is her
    /// decision, and Rabnest has no business inventing a second one.
    QColor colour{0x2B, 0x2B, 0x2B};
    QVector<qreal> dashes;
};

/// A print run: where every piece stands, and how much paper it takes.
///
/// **This is Ostara's print nesting** (`OExportNesting::Run` with `cuttingPlan` false): the same
/// engine, the same footprint rule, the same bin, the same grid handed to the engine. What it is
/// not is the cutting plan: no fold route, no twin, no constrained angles, no quantities — a print
/// run lays the pattern, and the cloth does not exist.
struct PrintPlan
{
    bool ok = false;
    QString error;

    /// The bin, and the height the nesting came out at.
    double binWidthPx = 0.0;
    double usedHeightPx = 0.0;

    double marginCm = 0.0;
    double gapCm = 0.0;

    /// The grid handed to the engine, and the tiling read back from the result.
    int columns = 1;
    double sheetHeightMm = 0.0;
    ExportArtboards::Grid grid;
    ExportSheets::Set sheets;

    QVector<PrintLayer> layers;

    /// One placement per footprint, and the origin its rotation is measured from — the min corner
    /// of that footprint, as everywhere in the socle.
    std::vector<Placement> placements;
    Footprint::Result footprint;
    QVector<QPointF> origins;

    /// The rectangle the cover page reserves, when its seat is InSheet — kept so that the sheets
    /// can be read again after a hand has moved a piece.
    QRectF reservedPx;

    /// Reads the sheets again from where the pieces ARE.
    ///
    /// The tiling does not move — its grid is the plan's — but the paper follows the ink: emptying
    /// a sheet by hand drops it, and putting a piece back on a dropped one brings its page back.
    /// This is the same `ExportSheets::Compute` the layout ran, so a moved piece and a laid one
    /// answer the same rule.
    void RefreshSheets();

    /// Lays the selected sizes out.
    ///
    /// @param sizeIds       the size layers the user kept, by their SVG id
    /// @param pieceNames    the pieces the user kept, by name
    /// @param marginCm      the document margin, as the print panel states it
    /// @param gapCm         the space between two pieces, as the print panel states it
    /// @param binWidthPx    the bin — columns × the sheet's own width, in pixels
    /// @param columns       sheets side by side; 0 or 1 for a roll
    /// @param sheetHeightMm the sheet's long side, in millimetres — 0 for a roll
    /// @param coverBlockPx  the rectangle the cover page reserves, when its seat is InSheet
    static PrintPlan Generate(const ImportedDocument &document, const QStringList &sizeIds,
                              const QStringList &pieceNames, double marginCm, double gapCm,
                              double binWidthPx, int columns, double sheetHeightMm,
                              const QRectF &coverBlockPx = QRectF());
};

#endif // PRINTPLAN_H
