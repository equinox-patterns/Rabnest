/************************************************************************
 **
 **  @file   printitems.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  What a printed plan draws under and around its pieces — Ostara's scene items, ported.
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

#ifndef PRINTITEMS_H
#define PRINTITEMS_H

#include "app/print/exportmarkpainter.h"
#include "core/print/exportartboards.h"
#include "core/print/exportcoverpage.h"
#include "core/print/exportsheets.h"
#include "core/print/exporttemplate.h"

#include <QByteArray>
#include <QGraphicsItem>
#include <QPointF>
#include <QRectF>
#include <QStyleOptionGraphicsItem>
#include <QtGlobal>

class QGraphicsScene;
class QPainter;

/// The four bands of the scene, named once — **Ostara's own Z values** (`oexportcanvas.cpp:85-106`).
///
/// A piece carries no Z of its own and sits at 0; the paper goes under everything, and the cut lines
/// over everything — because a cut line hidden under a cartouche is a cut line the user cannot cut
/// along.
///
/// The assembly marks sit between the paper and the pieces, and that band is not a matter of taste:
/// they are the plan's own ground, and a mark drawn over a piece would be a mark printed on the
/// fabric. The cover page sits over the marks — a page number written across the cover would be the
/// plan writing on someone else's paper — and under the pieces, because in the file the cover is
/// written in the underlay, under every layer.
namespace PrintBands
{
constexpr qreal kPaperZ = -2.0;
constexpr qreal kBinZ = -1.0;
constexpr qreal kAssemblyMarksZ = -0.5;
constexpr qreal kCoverZ = -0.25;
constexpr qreal kDecorZ = 1.0;
constexpr qreal kGridZ = 2.0;
} // namespace PrintBands

/// The sheets a tiled plan is cut into, drawn as the lines it is cut along.
///
/// One item for the whole grid rather than one rectangle per sheet: the sheets butt edge to edge, so
/// consecutive rectangles would draw every internal line twice.
///
/// **It is INERT** — it answers no mouse, takes no selection: a grid left answering would be taken
/// for a piece of the pattern.
class ArtboardGridItem : public QGraphicsItem
{
public:
    ArtboardGridItem(const QRectF &planRect, qreal cellWidthPx, qreal cellHeightPx, int columns,
                     int rows, const ExportSheets::Set &sheets);

    auto boundingRect() const -> QRectF override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    static constexpr qreal kPenWidth = 1.0;

    QRectF m_planRect;
    qreal m_cellWidthPx{0.0};
    qreal m_cellHeightPx{0.0};
    int m_columns{0};
    int m_rows{0};
    ExportSheets::Set m_sheets;
};

/// The assembly marks of a template, drawn on the plan's own ground.
///
/// One item for the whole plan, and drawn from the tiling as a whole: the frame is the grid's own
/// lines, each drawn once, and a mark sits where sheets meet rather than on each of them.
///
/// It OWNS the painter rather than borrowing one: A rebuild tears the scene down and builds it
/// again, and five SVG files parsed per rebuild cost nothing.
class AssemblyMarksItem : public QGraphicsItem
{
public:
    AssemblyMarksItem(const ExportTemplate &marks, const QPointF &origin,
                      const ExportMarkGrid &grid, bool framed);

    auto boundingRect() const -> QRectF override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    static constexpr qreal kPenWidth = 1.0;

    ExportMarkPainter m_decor;
    ExportMarkGrid m_grid;
    QPointF m_origin;
    bool m_framed{false};
};

/// The cover page, drawn at the size it will be printed.
///
/// **It resolves nothing.** Ostara paints bands resolved by OExportCoverPage::Of; Rabnest paints
/// the page the payload carried, whole, as a vector document — `CarryCover` wrote it into
/// `<exportCover>` at exactly the page's size (19 × 25.9 cm), and it is never resized here.
///
/// INERT, for the same reason the other two are.
class CoverPageItem : public QGraphicsItem
{
public:
    CoverPageItem(const QRectF &page, const QByteArray &svg);

    auto boundingRect() const -> QRectF override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF m_page;
    QByteArray m_svg;
};

/// The paper of a printed run: the printed sheets, white, and nothing drawn on them.
///
/// ONE RECTANGLE PER PRINTED SHEET, and not one over the plan. A cell with nothing on it is not
/// printed, and white paper drawn there would be the canvas promising a page the file will never
/// write — which is the one thing the canvas must not do.
void MakePaper(QGraphicsScene *scene, const QRectF &rect, const QPointF &origin = {});

void MakeArtboardPaper(QGraphicsScene *scene, const ExportArtboards::Grid &artboards,
                       const ExportSheets::Set &sheets, const QPointF &origin = {});

#endif // PRINTITEMS_H
