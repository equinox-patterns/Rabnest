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
#include <QVector>
#include <QWidget>

class QGraphicsScene;
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

    /// Shows one plan, and nothing else: the list of them is the dock's, not the canvas's.
    void Show(const CutPlan &plan);

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
    void RedrawPrint();

    /// A piece of the print run was moved, by @p delta in the scene's own coordinates. The
    /// placement it is drawn at takes the move — every size of that piece follows, since they share
    /// one footprint — and the sheets are read again.
    void MovePrintPiece(int placementIndex, const QPointF &delta);

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

private:
    void Draw(const CutPlan &plan);
    void ZoomBy(double factor);

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
};

#endif  // WORKSPACEPAGE_H
