/************************************************************************
 **
 **  @file   oexportmarkpainter.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   17/09/2026
 **
 **  @brief  Draws the assembly marks of a template - frame, scissors, marks and pagination.
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

#ifndef EXPORTMARKPAINTER_H
#define EXPORTMARKPAINTER_H

#include "app/print/exportmarkpainter.h"

#include "core/print/exportframesettings.h"
#include "core/print/exportmarksettings.h"
#include "core/print/exportpaginationsettings.h"
#include "core/print/exportsheets.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

class QPainter;
class QSvgRenderer;

/**
 * @brief The tiling the assembly marks are drawn on, as the drawn plan sees it.
 *
 * One sheet's size in pixels, how many of them side by side and how many stacked, the scale that
 * puts millimetres into pixels — everything a decor length needs to become a drawn length — and
 * which of those cells have paper.
 *
 * The sheets tile edge to edge, so the plan is at most this many cells: a cell with nothing on it
 * is not printed, and the decor follows the paper. No frame along its borders, no scissors on it,
 * no number on it, and no mark at a junction where it stood alone.
 *
 * This is the geometry the rendering side works in, and it is not `ExportArtboards::Grid`: that
 * one carries the plan's rectangle and its own validity rules, and belongs to the export. They
 * describe the same tiling seen from two sides, and the canvas maps one onto the other — which is
 * the only place the two meet.
 */
struct ExportMarkGrid
{
    int columns{0};
    int rows{0};

    /** One sheet, across and down, as drawn. */
    qreal sheetWidthPx{0.0};
    qreal sheetHeightPx{0.0};

    /** The scale in force. A decor length is absolute: it is never a fraction of the sheet. */
    qreal pxPerMm{0.0};

    /**
     * Which cells are printed — see ExportSheets.
     *
     * Required to cover this grid, and IsValid says so: a caller that leaves it half-filled would
     * otherwise get a decor drawn along the borders of sheets that are not there, silently.
     */
    ExportSheets::Set sheets{};

    auto IsValid() const -> bool
    {
        return columns > 0 && rows > 0 && sheetWidthPx > 0.0 && sheetHeightPx > 0.0 && pxPerMm > 0.0
               && sheets.columns == columns && sheets.rows == rows
               && sheets.ranks.size() == static_cast<qsizetype>(columns) * rows;
    }

    /** @brief True when the cell is one of the plan's printed sheets. */
    auto IsPresent(int row, int col) const -> bool { return sheets.IsPresent(row, col); }

    /** @brief The sheet's printed number, or 0 when it is not printed. */
    auto RankOf(int row, int col) const -> int { return sheets.RankOf(row, col); }

    /** @brief True when nothing is missing — every cell of the grid is printed. */
    auto IsWhole() const -> bool { return sheets.count == columns * rows; }

    /** @brief The stretches of the vertical boundary at @p boundary that have paper beside them. */
    auto VerticalEdges(int boundary) const -> QVector<ExportSheets::Run>
    {
        return sheets.VerticalEdges(boundary);
    }

    /** @brief The same for the horizontal boundary at @p boundary. */
    auto HorizontalEdges(int boundary) const -> QVector<ExportSheets::Run>
    {
        return sheets.HorizontalEdges(boundary);
    }
};

/**
 * @brief The one renderer of a template's assembly marks.
 *
 * **One renderer, several outputs.** The same decor is drawn in the template editor's preview, on
 * the Export canvas, and — later — written into the exported PDF. Drawing it once here is what
 * keeps the three from drifting apart: a mark that is placed from the plan in one place and from
 * the sheet in another is a mark that glues wrong, and no test would catch it. This is the
 * cartouche's pattern (`OCartouchePainter`), applied to the tiling.
 *
 * Four rules are the whole of what the decor is, and they are not preferences:
 *
 * - **Placed from the plan, not from the sheet.** Every line of the frame, every mark and every
 *   scissors is positioned against the tiling's own origin. A boundary between two sheets is one
 *   line, drawn once; a mark at a junction is one mark, drawn once. Drawing per sheet would put
 *   two lines where one belongs, restart the dash pattern at every edge, and double every mark.
 * - **The decor follows the paper.** A cell with nothing on it is not printed, and nothing of the
 *   decor is drawn on it or for it: a border of the frame that only such a cell would have carried
 *   goes with it, and a junction where it stood alone is no junction. What decides is which sheets
 *   are printed, which is read from the plan and not kept here — see ExportSheets.
 * - **Absolute sizes.** A length is in millimetres, points or pixels and stays that size on a
 *   small artboard as on a large one; `pxPerMm` is the only conversion.
 * - **The frame and its scissors belong to the framed formats alone.** Every other tiled model has
 *   its boundary along the paper's own edge, where a frame has nowhere to go. The marks and the
 *   pagination are drawn whatever the format: a sheet of A4 is glued to its neighbours too.
 *
 * The painter owns the SVG renderers of the three mark shapes and of the two scissors, so an
 * instance is neither copyable nor assignable. Construct one and keep it — building it per paint
 * would re-parse five SVG files on every repaint.
 *
 * See Modifications/Ostara/ExportMode/PDFTemplateEditor.md §3.4, §11 and §12.
 */
class ExportMarkPainter
{
public:
    ExportMarkPainter();
    ~ExportMarkPainter();

    ExportMarkPainter(const ExportMarkPainter &other) = delete;
    auto operator=(const ExportMarkPainter &other) -> ExportMarkPainter & = delete;

    /** @brief The frame and the scissors — the decor of the framed formats. */
    void SetFrameSettings(const ExportFrameSettings &frame);

    /** @brief The assembly marks — what they look like, and where they are set. */
    void SetMarkSettings(const ExportMarkSettings &marks);

    /** @brief The page number, and how it is written. */
    void SetPaginationSettings(const ExportPaginationSettings &pagination);

    /**
     * @brief Draws the whole decor of the template onto @p grid, from @p origin.
     *
     * @param framed     whether the format is one of the framed ones. On every other tiled
     *                   format the frame and the scissors are not drawn at all.
     * @param sheetGuide whether to stand in for a frame that is not drawn, with a faint outline
     *                   along the tiling's boundaries. The editor's preview says yes: it has
     *                   nothing else to tell its sheets apart, and there the outline is the only
     *                   thing between six white rectangles and one white block.
     *
     *                   The Export canvas says no, and the reason is the mirror image: it draws
     *                   its own cut lines over the pieces, which is where they are readable. It
     *                   keeps them exactly where the frame is absent, and stands them down where
     *                   the frame is drawn — a second line under the user's own would fight the
     *                   opacity they set, which is the defect the preview was cured of.
     */
    void Paint(QPainter &painter, const QPointF &origin, const ExportMarkGrid &grid, bool framed,
               bool sheetGuide) const;

private:
    /**
     * @brief The frame, drawn as the grid's own lines and not as one rectangle per sheet.
     *
     * A boundary between two sheets belongs to both, and is drawn once here — which is what makes
     * the sheets fuse. Every vertical line runs from the top of the grid downwards and every
     * horizontal one from its left edge rightwards, so the dash pattern has a single anchor for
     * the whole plan: a sheet's right edge and its neighbour's left edge are the same line, and
     * the two carry the same dashes at the same places. Drawn sheet by sheet, each rectangle
     * would restart its own pattern at its own corner, and the seams would show in print.
     */
    void DrawFrame(QPainter &painter, const QPointF &origin, const ExportMarkGrid &grid) const;

    /** @brief The boundaries of the tiling, each drawn once, with whatever pen is set. */
    void DrawGridLines(QPainter &painter, const QPointF &origin, const ExportMarkGrid &grid) const;

    /**
     * @brief The assembly marks, centred on the corners and the mid-edges of the tiling.
     *
     * Each one is drawn ONCE, where the sheets meet — half of it belongs to the sheet on either
     * side, a quarter to each of four at a corner. Corners where two sheets meet are marked as
     * well as those where four do, and there the mark is cut by the plan's edge: the part that
     * falls outside belongs to no sheet, and is not drawn.
     */
    void DrawMarks(QPainter &painter, const QPointF &origin, const ExportMarkGrid &grid) const;

    /** @brief The two scissors of one sheet, flat against the frame and inside it. */
    void DrawScissors(QPainter &painter, const QRectF &sheet, const ExportMarkGrid &grid) const;

    /**
     * @brief The page number, at the centre of every sheet.
     *
     * The one thing of the decor that is drawn **per sheet** rather than once for the plan: a
     * number belongs to the sheet it is printed on, and no two sheets carry the same. That is
     * also why the row and the column are read here and nowhere else — the frame, the scissors
     * and the marks are placed from the plan and would have no use for them.
     */
    void DrawCentreNumber(QPainter &painter, const QPointF &origin,
                          const ExportMarkGrid &grid) const;

    /**
     * @brief The page number at the corners of the sheets, where sheets meet.
     *
     * One number per junction, printed by every sheet that meets there, each in its own corner
     * and inside itself. Two sheets that touch therefore carry the same number at the place where
     * they touch, and gluing them is a matching game rather than a reckoning.
     *
     * Where a junction has no number to give — a corner of the plan — nothing is drawn, and the
     * sheet keeps that corner empty. The whole rule is written out in
     * ExportPaginationPlacement.
     */
    void DrawCornerNumbers(QPainter &painter, const QPointF &origin,
                           const ExportMarkGrid &grid) const;

    /**
     * @brief One orientation of the edge-centre numbers — the middle of the seams between sheets.
     *
     * Called once for the seams running down the plan and once for those running across it, each
     * with its own settings and its own box. The two differ in nothing but orientation: which
     * seams are walked, which sheets meet there, and which way « across the seam » points.
     *
     * A number reads the marks' **size** and not their visibility, and is held between two rails
     * — never inside the printer's margin, never off the sheet. The whole rule is written out in
     * ExportPaginationPlacement.
     */
    void DrawEdgeNumbers(QPainter &painter, const QPointF &origin, const ExportMarkGrid &grid,
                         bool vertical) const;

    /**
     * @brief The text of one sheet's page number.
     *
     * **The two forms are not the same kind of thing.** The two-axis form NAMES a place: the
     * letter is the column and the number is the row, so nothing is ever renumbered there — a
     * letter that stopped naming its column, or a number that stopped naming its row, would not
     * be a grid any more.
     *
     * The number alone COUNTS. It is the sheet's rank among the sheets that are printed, in
     * reading order, so a sheet dropped from the plan takes its number with it and every sheet
     * after it moves up one — 1, 2, 3 with no gap. On a plan with nothing missing the rank is
     * `row * columns + col + 1`, which is what this has always written.
     */
    auto SheetText(const ExportMarkGrid &grid, int row, int col) const -> QString;

    /**
     * @brief The number a junction carries, or an empty string when it carries none.
     *
     * A junction is where sheets MEET, and a number is what registers them against one another.
     * So a junction at which fewer than two printed sheets stand carries none: the four corners
     * of the plan, and now the corner of a hole where a single sheet stands alone. Where two or
     * more do meet, the junction takes the number of the first of them in reading order — which
     * is the sheet above and to the left wherever there is one.
     */
    auto JunctionNumber(const ExportMarkGrid &grid, int anchorCol, int anchorRow) const -> QString;

    /**
     * @brief Sets the painter's font and colour from one placement's text style.
     *
     * Shared by the placements, which differ in where they draw and in nothing else: a second
     * copy of the absolute-size conversion is a second place for it to drift.
     */
    void ApplyTextStyle(QPainter &painter, const ExportPaginationTextStyle &style,
                        const ExportMarkGrid &grid) const;

    ExportFrameSettings m_frame{};
    ExportMarkSettings m_marks{};
    ExportPaginationSettings m_pagination{};

    /**
     * One renderer per row of the shape table, in the table's own order — which is also the
     * order of ExportMarkShape's values, and the order the combo offers. The three must stay
     * in step; the drawing checks the index before reaching for one.
     */
    std::vector<std::unique_ptr<QSvgRenderer>> m_shapeRenderers;

    /** The two scissors marks, drawn at the size their files declare. */
    std::unique_ptr<QSvgRenderer> m_scissorsVertical;
    std::unique_ptr<QSvgRenderer> m_scissorsHorizontal;
};

#endif // EXPORTMARKPAINTER_H
