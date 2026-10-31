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

class ImportedDocument;

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

    /// The size whose decor is drawn — **the one the payload NAMES**, never the first of the run.
    /// Every size draws its own ink; only this one carries a cartouche, an annotation or a grain
    /// line, exactly as Ostara's canvas does.
    bool reference{false};

    /// A size the run NEEDED and the user did not ask for: the payload's reference size.
    ///
    /// **Its pieces are not drawn; its drawing is.** The children are written in the frame of that
    /// size, so a run that leaves it out has no anchor to seat them on — and anchoring them on the
    /// smallest size it DID get shifts a fold piece's cartouche by exactly the growth between the
    /// two, which is what « tout est décalé depuis le pli » was. Keeping it costs nothing: the
    /// smallest size of a graded pattern lies inside every other one, so it changes no footprint.
    bool hidden{false};

    /// How this size is DRAWN — its line colour and its dash pattern, read from the payload's own
    /// layer for it.
    ///
    /// A graded plan puts several sizes on one sheet, and Ostara tells them apart by the style the
    /// pattern gives each size; the payload's drawing carries it (`<g id="Taille_46">`, whose paths
    /// hold `stroke` and `stroke-dasharray`). Read, never computed: the creator's palette is her
    /// decision, and Rabnest has no business inventing a second one.
    QColor colour{0x2B, 0x2B, 0x2B};
    QVector<qreal> dashes;

    /// The weight the size's own paths state, in the payload's pixels.
    ///
    /// **The canvas does not draw with it**, and does not want to: a plan is shown zoomed out, and
    /// a 0.5 mm line at that scale is a grey smear — the canvas has a floor of its own. A FILE has
    /// no such problem and prints on paper, so it draws with the weight the pattern was drawn with.
    /// Zero when the payload states none.
    qreal width = 0.0;
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

    /// The format the run was laid for, by its token — what the canvas names the run by, and the
    /// only thing that says which paper the sheets are.
    QString modelToken;

    /// **Black and solid, whatever the pattern's own line says.** Asked for when the run prints a
    /// SINGLE size: a size's colour and dashes exist to tell sizes apart, and with one size they tell
    /// nothing — they only make a real pattern's fine dots (0,46 mm of ink every 2,82 mm) disappear
    /// at the scale a plan is shown. Kept in the run, so the canvas and any file drawn from it answer
    /// the same.
    bool plainOutline = false;

    /// **Ce plan est une PROJECTION**, et son trait est celui du format, pas celui du patron.
    ///
    /// Décidé ici, une fois, pour que le canevas et le fichier ne puissent pas diverger : les coupons
    /// en noir plein à 7 pt, le droit-fil à 5,25, le pli à 4. Un trait appliqué seulement à l'écriture
    /// laisserait l'écran montrer le trait du patron, et c'est exactement ce qui se voyait.
    bool projector = false;

    /// How long the laying out took, in seconds, measured around the engine. The figure the user
    /// reads is the time HER plan cost, so it travels inside the plan rather than being counted by
    /// whoever happened to call it.
    double seconds = 0.0;

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

    /// Les rotations que le moteur a le droit d'essayer, par clé d'empreinte — **vide pour un print**,
    /// qui laisse au moteur son balayage entier.
    ///
    /// Une projection ÉPINGLE le droit-fil (`grainPinned`) : sans cela le moteur essaie toutes les
    /// rotations sur chaque pièce, et une page de cent trente pièces coûte des minutes au lieu de
    /// secondes. C'est la règle d'Ostara, et c'est aussi ce qui fait qu'un plan projeté se lit dans
    /// le sens du tissu.
    QHash<QString, std::vector<double>> angles;

    /// The rectangle the cover page reserves, when its seat is InSheet — kept so that the sheets
    /// can be read again after a hand has moved a piece.
    QRectF reservedPx;

    /// How many PAGES the run prints: one per sheet that carries ink — or ONE for a roll and for the
    /// A0 sheet, which are not cut into sheets at all — plus the cover page when it stands on a
    /// sheet of its own.
    ///
    /// The one place this is answered: the canvas writes it under the drawing, the info dialogue
    /// counts it, and the command-line tool prints it. Three readings of « how many sheets does this
    /// run take » would be three answers waiting to disagree.
    auto Pages(bool coverHasItsOwnSheet) const -> int;

    /// Le cadre serré de ce que le plan occupe, en pixels — la page qu'une projection mesure.
    ///
    /// **La même arithmétique que le dessin et que le tuilage** : `(x, y) + R(rotation) · (p − origin)`,
    /// lue sur les empreintes placées. Une projection s'en sert pour remplacer son bac par sa page ;
    /// une seconde écriture de cette transformation ferait deux réponses à « où est la pièce ».
    auto InkBounds() const -> QRectF;

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
    /// @param modelToken    the format's token, kept as the run's own fact
    /// @param plainOutline  true to draw every piece black and solid instead of with the pattern's
    ///                      own line — the user's choice, and only ever offered for a single size
    /// @param ungrouped  the groups the user chose NOT to keep superimposed — see @ref CompleteGroup.
    ///                   Each of their members is then an ordinary piece: its own drawing, its own
    ///                   footprint, laid apart. Empty means « keep every group as the file has it ».
    /// @param cloth    what the run does with a piece cut on the fold — @see Cloth. A print passes
    ///                 @ref Cloth::AsDrawn; a projection passes @ref Cloth::Flat, because it shows the
    ///                 pieces the way they are CUT, unfolded at the fold.
    /// @param perInstance  one footprint per POSE instead of one per piece — what a projection lays
    ///                 out, since a piece cut twice is two pieces to project. A print groups the
    ///                 poses of one piece under one name, which is what makes a graded plan graded.
    /// @param centred  centre the sizes of a piece on each other instead of keeping the payload's own
    ///                 alignment — what a projection forces (`Projector.md` §6.1). A print never
    ///                 does: the pattern's alignment is the drawing the document has.
    static PrintPlan Generate(const ImportedDocument &document, const QStringList &sizeIds,
                              const QStringList &pieceNames, double marginCm, double gapCm,
                              double binWidthPx, int columns, double sheetHeightMm,
                              const QRectF &coverBlockPx = QRectF(),
                              const QString &modelToken = QString(),
                              bool plainOutline = false,
                              const QSet<QString> &ungrouped = QSet<QString>(),
                              Cloth cloth = Cloth::AsDrawn, bool perInstance = false,
                              bool centred = false, bool grainPinned = false,
                              bool projector = false);
};

#endif // PRINTPLAN_H
