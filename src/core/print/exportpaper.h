/************************************************************************
 **
 **  @file   exportpaper.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   24/10/2026
 **
 **  @brief  Which sheets of paper a print run can be printed on.
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

#ifndef EXPORTPAPER_H
#define EXPORTPAPER_H

#include <QPageSize>
#include <QSizeF>
#include <QString>
#include <QVector>

/// What a print run may be printed ON — and it is not a taste.
///
/// **A run's pages are already at their final size.** A pattern printed on A4 is cut into A4 sheets,
/// a framed run into a sheet ten millimetres inside A4, and a sheet of either is glued to its
/// neighbours: printing one at another size, or shrunk to fit, gives a plan that looks right and
/// assembles wrong. So the paper is not asked for — it is deduced: a sheet can be printed on a paper
/// **that holds it whole**, and on no other.
///
/// The deduction reproduces the formats one by one, without a table to keep in agreement with them:
/// an A4 sheet is held by A4 alone (Letter is shorter), a Letter sheet by Letter alone (A4 is
/// narrower), a framed sheet by both, an A3 by A3 alone, an A3/Tabloid framed by both, an A0 by A0.
namespace ExportPaper
{
/// The pair of papers a format is printed on.
///
/// **A format belongs to one pair, and never to a paper.** An A4 run is an A4 run and not an « A4 or
/// anything bigger » run: its tiling is A4 sheets, and a sheet of paper that is not in its pair is a
/// tiling of another size that happens to hold the same drawing.
enum class Family
{
    A0,
    A3Tabloid,
    A4Letter
};

/// One paper, as the printer names it and as a reader measures it.
struct Paper
{
    QPageSize::PageSizeId id{QPageSize::A4};
    QSizeF sizeMm;
};

/// The papers of @p family that hold a sheet of @p sheetMm whole, the smallest first.
///
/// One for every format whose sheet IS a paper — A4, Letter, A3, Tabloid, A0 — and two for the framed
/// formats, whose sheet is the paper less ten millimetres all round and so fits both members of its
/// pair. Empty means the sheet is larger than both: a roll run longer than an A0 sheet is the case,
/// and it is refused rather than cropped, because a sheet cut short no longer glues to its neighbour.
auto Fitting(Family family, const QSizeF &sheetMm) -> QVector<Paper>;

/// What a paper is called on screen, in the reader's language: « A4 », « Letter », « Tabloid ».
auto Name(QPageSize::PageSizeId id) -> QString;
} // namespace ExportPaper

#endif // EXPORTPAPER_H
