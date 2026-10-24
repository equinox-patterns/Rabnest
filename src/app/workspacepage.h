/************************************************************************
 **
 **  @file   workspacepage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The canvas the cutting plan is drawn on.
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

#ifndef WORKSPACEPAGE_H
#define WORKSPACEPAGE_H

#include "core/cutplan.h"
#include "core/print/exporttemplate.h"
#include "core/print/printplan.h"

#include <QByteArray>
#include <QHash>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QGraphicsScene;
class QGraphicsItem;
class QGraphicsView;
class QPainter;
class QLabel;
class QListWidget;

/// The plan, drawn. One canvas, one list — one plan per size.
///
/// **Nothing leaves this page.** A cutting plan is what Ostara's own mode shows: pieces laid on a
/// length of fabric, the length written at the bottom. There is no file to write, so there is no
/// button to write one, and the canvas is the whole of the result.
class WorkspacePage : public QWidget
{
    Q_OBJECT

public:
    /// The thinnest a piece's stroke may be drawn on SCREEN, in scene pixels: one, so a plan fitted
    /// to the window — about a fifth of its size — is still drawn rather than guessed at.
    static constexpr double kLineFloorPx = 1.0;

    explicit WorkspacePage(QWidget *parent = nullptr);

    /// Shows one plan — and its LINING, when the run asked for a pair.
    ///
    /// **Ostara's own seat: the lining to the RIGHT of the fabric, one inch away** (`kLiningGapPx`,
    /// `oexportoutputs.cpp:103`). One scene for both, and that is the whole point: one zoom frames
    /// them together — you read them side by side, which is how a pair is read — and the figure line
    /// under the canvas speaks of one run, not of two.
    ///
    /// @param lining the lining plan, or null. A plan that is not `ok` is a size nobody lined and is
    ///        drawn as nothing at all.
    void Show(const CutPlan &plan, const CutPlan *lining = nullptr);

    /// Shows a PRINT run, as Ostara's own export canvas shows it: the sheets, their paper, the
    /// frame, the scissors, the marks, the numbers, and the cover page at its seat.
    ///
    /// **The same drawing as Ostara's, and drawn by the same code**: the decor is
    /// ExportMarkPainter, the paper and the grid are its scene items, and none of them is
    /// re-implemented here.
    ///
    /// The two cover rectangles are the caller's, because the seat is a property of the FORMAT:
    /// @p coverPaper is the sheet the page stands on (empty unless it needs one of its own), and
    /// @p coverPage is the page itself — 19 × 25.9 cm, never resized here.
    void ShowPrint(const PrintPlan &plan, const ExportTemplate &decor, bool framed,
                   const QByteArray &coverSvg, const QRectF &coverPaper, const QRectF &coverPage);

    /// Redraws the print run as it now stands — called after a hand moved a piece, because the
    /// paper follows the ink: a sheet a hand has emptied is dropped, and one it put a piece back on
    /// is printed again.
    ///
    /// @param refit true puts the run on screen fitted to the canvas — what a NEW run gets. False
    ///        keeps the view exactly where the hand left it, which is what a move needs: re-fitting
    ///        after a gesture would throw away the zoom taken before it.
    void RedrawPrint(bool refit = true);

    /// A piece of the print run was moved, by @p delta in the scene's own coordinates. The
    /// placement it is drawn at takes the move — every size of that piece follows, since they share
    /// one footprint — and the sheets are read again.
    void MovePrintPiece(int placementIndex, const QPointF &delta);

    /// The print run exactly as it is drawn, hand moves included.
    ///
    /// **The canvas is where a run is edited, and the caller is where it is kept.** A move changes
    /// this page's own copy; the row in the dock holds the one everything else reads — the file, the
    /// printer, the information dialogue — so the two have to be one value from time to time, and
    /// this is the read that does it.
    auto PrintPlanShown() const -> const PrintPlan & { return m_printPlan; }

    /// A blank canvas — « back to home » leaves nothing of the PDF on screen.
    void Clear();

    /// The plan as a FILE is made: the scene drawn from @p sourcePx into @p targetPx.
    ///
    /// **The drawing is the panel's, and only the scale changes.** The canvas draws pieces with a
    /// stroke whose thickness follows the zoom — see `PatternPen` — so a file, drawn at another
    /// scale, must re-lay that thickness for its own before rendering, and put the view's back
    /// afterwards. That is the whole reason this lives here and not in the writer.
    ///
    /// @param sourcePx the plan's own rectangle, never `sceneRect`: the twenty pixels of margin
    ///        around the drawing are a margin of the VIEW, and a file that carried them would grow a
    ///        white border nobody asked for
    /// @param floorPx the thinnest a stroke may be drawn, in SCENE pixels — one on screen, a length
    ///        in millimetres on paper, where a device pixel is a quarter of a hair
    void RenderPlan(QPainter *painter, const QRectF &sourcePx, const QRectF &targetPx,
                    double floorPx = kLineFloorPx);

    /// The zoom of the canvas, in steps around what is on screen.
    ///
    /// **A step, not a factor of the plan.** Zooming does not re-lay anything: it changes how the
    /// same drawing is looked at, and a plan shown at 150 % is the plan, seen closer.
    void ZoomIn();
    void ZoomOut();

signals:
    /// A hand moved a piece of the run shown. **The run is now this page's, and not the row's**: it
    /// is emitted so that whoever keeps the row can take the edited run back before anything reads
    /// it, and so that selecting the row again cannot put the old positions back on screen.
    void PrintPlanEdited();

protected:
    /// **A canvas that was resized is a canvas that no longer fits.** `fitInView` is applied once,
    /// when a plan is put on screen, and the widget around it moves afterwards — a tab shown, a
    /// window resized, the line under the plan growing or shrinking to its text — so the drawing
    /// kept the previous scale and the bottom of it fell outside the viewport: a plan whose last
    /// row is cut. The view's own viewport is watched, and the fit is taken again every time it
    /// changes size, unless a hand has zoomed — a zoom is not undone by a resize.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void Draw(const CutPlan &plan, const CutPlan *lining);

    /// Draws ONE cloth at @p seat, and returns the rectangle it occupies in the scene.
    ///
    /// The seat is the only thing that tells the two cloths apart on screen: the same code draws
    /// both, so the lining carries its pieces, its ink, its decor and its children exactly as the
    /// fabric does, and a defect cannot exist on one side and not the other.
    auto DrawCloth(const CutPlan &plan, const QPointF &seat) -> QRectF;

    /// The figures under the canvas. One cloth reads as it has always read; a pair names its two
    /// halves, because two widths and two lengths in a row would otherwise be a riddle.
    auto FiguresText(const CutPlan &plan, const CutPlan *lining) const -> QString;

    /// One cloth's figures: the half width when it is folded, the width and the length in
    /// centimetres, then the same two in inches and yards.
    auto ClothFigures(const CutPlan &plan) const -> QString;
    void ZoomBy(double factor);

    /// Fits the whole scene in the viewport, and re-lays the strokes for that scale.
    void FitToScene();

    /// The gesture that moves a piece of a print run: a press on a footprint, its steps, and the
    /// placement it lands on. Read on the viewport, before a view in `ScrollHandDrag` can pan with
    /// it — the item is moved by the page, never by itself.
    ///
    /// @return true when the event was the gesture's and must not go further.
    bool HandlePrintDrag(QEvent *event);

    /// Re-lays every piece's stroke for the zoom the view is at — see @ref PatternPen. Called after
    /// anything that moves the view's transform, and never on a timer: a stroke one frame behind the
    /// wheel is a stroke that thins out while the hand is still turning it.
    void RefreshLineWidth();

    /// The same, for a scale that is not the view's — a file being written, a page being printed.
    void RefreshLineWidth(double scale, double floor);

    QGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QLabel *m_detail = nullptr;

    /// The print run on screen, and what drew it — kept so a moved piece can be redrawn without
    /// re-laying anything.
    PrintPlan m_printPlan;
    ExportTemplate m_printDecor;
    QByteArray m_coverSvg;
    QRectF m_coverPaper;
    QRectF m_coverPage;
    bool m_printFramed = false;
    bool m_printMode = false;

    /// What is laid WITH a footprint — the cartouche, the annotations, the miniature, the grain
    /// line of its reference size — so that a gesture moves the drawing and not only the outline.
    QHash<int, QVector<QGraphicsItem *>> m_printFollowers;

    /// The footprints on screen, and the gesture in progress: what was taken, where the hand was,
    /// and how far it has gone.
    QVector<QGraphicsItem *> m_printItems;
    QGraphicsItem *m_dragItem = nullptr;
    QPointF m_dragLast;
    QPointF m_dragTotal;

    /// True while the SPACE key is held: the canvas is being panned, and a click is a pan and not a
    /// grip — a hand that holds space to move around must not drag a piece out of the plan on the
    /// way.
    bool m_spaceHeld = false;

    /// True once a hand has zoomed: a resize must then leave the scale alone.
    bool m_zoomed = false;

    /// Guards the re-fit against the resize its own scrollbars cause.
    bool m_fitting = false;
};

#endif  // WORKSPACEPAGE_H
