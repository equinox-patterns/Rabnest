/************************************************************************
 **
 **  @file   printitems.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's artboard grid, marks host and paper, ported to Rabnest's scene.
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

#include "app/print/printitems.h"

#include <QBrush>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QPen>
#include <QSvgRenderer>

//---------------------------------------------------------------------------------------------------------------------
ArtboardGridItem::ArtboardGridItem(const QRectF &planRect, qreal cellWidthPx, qreal cellHeightPx,
                                   int columns, int rows, const ExportSheets::Set &sheets)
  : m_planRect(planRect), m_cellWidthPx(cellWidthPx), m_cellHeightPx(cellHeightPx), m_columns(columns),
    m_rows(rows), m_sheets(sheets)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(PrintBands::kGridZ);
}

//---------------------------------------------------------------------------------------------------------------------
auto ArtboardGridItem::boundingRect() const -> QRectF
{
    // The plan's outer edge is drawn ON the rect, so half the pen falls outside it — rounded up to
    // the whole of it rather than left to a repaint that never comes.
    return m_planRect.adjusted(-kPenWidth, -kPenWidth, kPenWidth, kPenWidth);
}

//---------------------------------------------------------------------------------------------------------------------
void ArtboardGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(QPen(Qt::black, kPenWidth));

    // Nothing missing: the rectangle and its seams, drawn exactly as they always have been. The
    // outline goes through drawRect, which mitres its four corners, where four separate lines would
    // each poke half a pen past them.
    if (m_sheets.count == m_columns * m_rows)
    {
        painter->drawRect(m_planRect);

        for (int i = 1; i < m_columns; ++i)
        {
            const qreal x = m_planRect.left() + i * m_cellWidthPx;
            painter->drawLine(QPointF(x, m_planRect.top()), QPointF(x, m_planRect.bottom()));
        }

        for (int j = 1; j < m_rows; ++j)
        {
            const qreal y = m_planRect.top() + j * m_cellHeightPx;
            painter->drawLine(QPointF(m_planRect.left(), y), QPointF(m_planRect.right(), y));
        }

        return;
    }

    // A sheet was dropped, so the lines stop where the paper does. Each boundary is drawn in the
    // stretches that have a printed sheet beside them, and drawn ONCE.
    for (int i = 0; i <= m_columns; ++i)
    {
        const qreal x = m_planRect.left() + i * m_cellWidthPx;

        for (const ExportSheets::Run &run : m_sheets.VerticalEdges(i))
        {
            painter->drawLine(QPointF(x, m_planRect.top() + run.first * m_cellHeightPx),
                              QPointF(x, m_planRect.top() + (run.last + 1) * m_cellHeightPx));
        }
    }

    for (int j = 0; j <= m_rows; ++j)
    {
        const qreal y = m_planRect.top() + j * m_cellHeightPx;

        for (const ExportSheets::Run &run : m_sheets.HorizontalEdges(j))
        {
            painter->drawLine(QPointF(m_planRect.left() + run.first * m_cellWidthPx, y),
                              QPointF(m_planRect.left() + (run.last + 1) * m_cellWidthPx, y));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
AssemblyMarksItem::AssemblyMarksItem(const ExportTemplate &marks, const QPointF &origin,
                                     const ExportMarkGrid &grid, bool framed)
  : m_grid(grid), m_origin(origin), m_framed(framed)
{
    m_decor.SetFrameSettings(marks.frame);
    m_decor.SetMarkSettings(marks.marks);
    m_decor.SetPaginationSettings(marks.pagination);

    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(PrintBands::kAssemblyMarksZ);
}

//---------------------------------------------------------------------------------------------------------------------
auto AssemblyMarksItem::boundingRect() const -> QRectF
{
    // The plan's outer edge is drawn ON the grid, so half the frame's pen falls outside it.
    const QRectF plan(m_origin, QSizeF(m_grid.columns * m_grid.sheetWidthPx,
                                       m_grid.rows * m_grid.sheetHeightPx));
    return plan.adjusted(-kPenWidth, -kPenWidth, kPenWidth, kPenWidth);
}

//---------------------------------------------------------------------------------------------------------------------
void AssemblyMarksItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // No stand-in outline is asked for: this scene draws its own cut lines, and it stands them down
    // itself wherever the frame is drawn.
    m_decor.Paint(*painter, m_origin, m_grid, m_framed, false);
}

//---------------------------------------------------------------------------------------------------------------------
CoverPageItem::CoverPageItem(const QRectF &page, const QByteArray &svg) : m_page(page), m_svg(svg)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(PrintBands::kCoverZ);
}

//---------------------------------------------------------------------------------------------------------------------
auto CoverPageItem::boundingRect() const -> QRectF
{
    return m_page;
}

//---------------------------------------------------------------------------------------------------------------------
void CoverPageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // **Scale 1 — the page is drawn at the size it will be printed.** The renderer lays its own
    // viewBox into the page's rectangle, which is the page's own size in both applications: the
    // cover is 19 × 25.9 cm and the sheet it stands on is never allowed to resize it.
    QSvgRenderer renderer(m_svg);
    if (renderer.isValid())
    {
        renderer.render(painter, m_page);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MakePaper(QGraphicsScene *scene, const QRectF &rect, const QPointF &origin)
{
    auto *paper = new QGraphicsRectItem(rect);
    paper->setPen(Qt::NoPen);
    paper->setBrush(QBrush(Qt::white));
    paper->setFlag(QGraphicsItem::ItemIsSelectable, false);
    paper->setFlag(QGraphicsItem::ItemIsMovable, false);
    paper->setAcceptedMouseButtons(Qt::NoButton);
    paper->setZValue(PrintBands::kPaperZ);
    paper->setPos(origin);
    scene->addItem(paper);
}

//---------------------------------------------------------------------------------------------------------------------
void MakeArtboardPaper(QGraphicsScene *scene, const ExportArtboards::Grid &artboards,
                       const ExportSheets::Set &sheets, const QPointF &origin)
{
    for (int row = 0; row < artboards.rows; ++row)
    {
        for (int col = 0; col < artboards.columns; ++col)
        {
            if (!sheets.IsPresent(row, col))
            {
                continue;
            }

            // A cell is a sheet, and a sheet is what MakePaper draws. The cover page's extra sheet
            // is the same call with a rectangle that is NOT a cell.
            MakePaper(scene,
                      QRectF(artboards.planRect.left() + col * artboards.cellWidthPx,
                             artboards.planRect.top() + row * artboards.cellHeightPx,
                             artboards.cellWidthPx, artboards.cellHeightPx),
                      origin);
        }
    }
}
