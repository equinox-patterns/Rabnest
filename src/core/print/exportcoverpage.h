/************************************************************************
 **
 **  @file   exportcoverpage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Where a cover page stands on a print model — Ostara's own geometry, ported.
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

#ifndef EXPORTCOVERPAGE_H
#define EXPORTCOVERPAGE_H

#include <QRectF>
#include <QSizeF>

#include "exportprintmodel.h"

/// @brief Where the cover page of a format stands, and how big it is.
///
/// **Ostara's `OExportCoverPage` geometry, ported** (`oexportcoverpage.cpp:54-108`). Rabnest keeps
/// no cover template and fills no band: the page it draws is the one the imported PDF carried, as
/// a whole. What it still needs is the two answers that belong to the FORMAT — where the page
/// stands, and on which rectangle of paper — because those do not travel in the page itself.
///
/// The page is 19 × 25.9 cm wherever it goes. That is the one figure here that does not follow
/// from the format: it is Ostara's `kPageWidthCm` / `kPageHeightCm`, and it never changes with the
/// paper the page lands on.
namespace ExportCoverPage
{
/// @brief The seat @p model gives a cover page. A null model answers None.
auto SeatOf(const ExportPrintModel *model) -> CoverSeat;

/// @brief The page's own rectangle, at PRINT SIZE — 19 × 25.9 cm in pixels.
auto PageSize() -> QSizeF;

/// @brief The block a reserved cover page occupies — the page's own rectangle, at the document
///        margin from the plan's top-left corner. Empty unless the seat is InSheet.
auto BlockPx(CoverSeat seat, qreal marginPx) -> QRectF;

/// @brief The rectangle the page stands on, in PIXELS.
///
/// InSheet: the block, at the plan's corner, off the edges by the document margin. ExtraSheet: one
/// sheet above row 1, column 1 — built from the grid's own cell size, never from a second reading
/// of the format table, so a run and its canvas cannot disagree about how wide a sheet is.
auto PaperPx(CoverSeat seat, qreal marginPx, const QRectF &planRect, qreal sheetWidthPx,
             qreal sheetHeightPx) -> QRectF;

/// @brief The page itself, centred on its paper.
auto PagePx(CoverSeat seat, qreal marginPx, const QRectF &planRect, qreal sheetWidthPx,
            qreal sheetHeightPx) -> QRectF;
} // namespace ExportCoverPage

#endif // EXPORTCOVERPAGE_H
