/************************************************************************
 **
 **  @file   exportcoverpage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's cover-page geometry, ported.
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

#include "exportcoverpage.h"

#include "printmetrics.h"

namespace
{
/// Ostara's `OCoverPageLayout::kPageWidthCm` / `kPageHeightCm` (`ocoverpagelayout.h:60-61`).
///
/// **The page is the feature's, not the preview's**: a cover page is 19 × 25.9 cm, always, and
/// every placement below is written against that one size rather than against the paper it lands
/// on.
constexpr qreal kPageWidthMm = 190.0;
constexpr qreal kPageHeightMm = 259.0;
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportCoverPage::SeatOf(const ExportPrintModel *model) -> CoverSeat
{
    // A null model is a token the table has since lost — a payload naming a format this build no
    // longer has. It draws nothing rather than guessing a seat it cannot check.
    return model == nullptr ? CoverSeat::None : model->coverSeat;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportCoverPage::PageSize() -> QSizeF
{
    return {PrintMetrics::MmToPx(kPageWidthMm), PrintMetrics::MmToPx(kPageHeightMm)};
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportCoverPage::BlockPx(CoverSeat seat, qreal marginPx) -> QRectF
{
    if (seat != CoverSeat::InSheet)
    {
        return {};
    }

    // The page's own rectangle, off the edges by the DOCUMENT's margin — the figure the user set,
    // and not a second margin invented for the block. The block was an A4's rectangle at the
    // absolute corner first, and it gave the nesting away for nothing.
    //
    // At the plan's top-left corner: y grows downward, so `marginPx` from the top is the margin.
    const QSizeF page = PageSize();
    return {marginPx, marginPx, page.width(), page.height()};
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportCoverPage::PaperPx(CoverSeat seat, qreal marginPx, const QRectF &planRect,
                              qreal sheetWidthPx, qreal sheetHeightPx) -> QRectF
{
    switch (seat)
    {
        case CoverSeat::InSheet:
            return BlockPx(seat, marginPx).translated(planRect.topLeft());

        case CoverSeat::ExtraSheet:
            // Above row 1, column 1 — one sheet of the grid's own size. Negative Y is above the
            // plan, and that is all it is: `planRect` — the fence a hand may not cross — is
            // deliberately NOT grown upward with it, or a piece could be dragged onto the cover
            // page.
            return {planRect.left(), planRect.top() - sheetHeightPx, sheetWidthPx, sheetHeightPx};

        case CoverSeat::None:
            break;
    }

    return {};
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportCoverPage::PagePx(CoverSeat seat, qreal marginPx, const QRectF &planRect,
                             qreal sheetWidthPx, qreal sheetHeightPx) -> QRectF
{
    const QRectF paper = PaperPx(seat, marginPx, planRect, sheetWidthPx, sheetHeightPx);
    if (paper.isEmpty())
    {
        return {};
    }

    // The page's own size, and never a share of the paper: the cover page is 19 × 25.9 cm at
    // 100 %, which is what makes the canvas evidence about the paper rather than a picture of it.
    // A paper too small would let it overflow, centred — honest, and no format does it.
    const QSizeF page = PageSize();

    return {paper.left() + (paper.width() - page.width()) / 2.0,
            paper.top() + (paper.height() - page.height()) / 2.0, page.width(), page.height()};
}
